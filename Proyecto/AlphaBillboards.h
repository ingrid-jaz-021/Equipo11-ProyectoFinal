#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.h"
#include "Texture2D.h"
#include "RenderMetrics.h"
#include "Frustum.h"

class AlphaBillboards
{
public:
    struct Texture {GLuint id;int width,height;};
    struct Instance {size_t texture;glm::mat4 model;glm::vec3 center,tint,extent;};
    AlphaBillboards():shader("Shader/billboard.vs","Shader/billboard.frag"),
        instancedShader("Shader/billboard_instanced.vs","Shader/billboard_instanced.frag")
    {
        // Anclaje inferior central y UV de PNG (origen superior izquierdo).
        const float vertices[]={-.5f,0,0,0,1, .5f,0,0,1,1, .5f,1,0,1,0, -.5f,1,0,0,0};
        const GLuint indices[]={0,1,2,0,2,3};
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ebo);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
        glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
        shader.Use();glUniform1i(shader.Uniform("texture_diffuse1"),0);
        glGenBuffers(1,&instanceVbo);
        instancedShader.Use();glUniform1i(instancedShader.Uniform("texture_diffuse1"),0);
    }
    ~AlphaBillboards()
    {
        for(const auto& texture:textures) glDeleteTextures(1,&texture.id);
        glDeleteBuffers(1,&ebo);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);
        glDeleteBuffers(1,&instanceVbo);
    }
    AlphaBillboards(const AlphaBillboards&)=delete;
    AlphaBillboards& operator=(const AlphaBillboards&)=delete;
    size_t Load(const std::string& file)
    {
        Texture t{};t.id=LoadRgbaTexture(file,GL_CLAMP_TO_EDGE,&t.width,&t.height,true);
        textures.push_back(t);return textures.size()-1;
    }
    float Aspect(size_t texture) const {const auto& t=textures.at(texture);return static_cast<float>(t.width)/t.height;}
    size_t Add(size_t texture,const glm::vec3& base,float yaw,float height,const glm::vec3& tint=glm::vec3(.82f,.83f,.87f),float width=0)
    {
        glm::mat4 model=glm::translate(glm::mat4(1),base);
        model=glm::rotate(model,yaw,glm::vec3(0,1,0));
        model=glm::scale(model,glm::vec3(width>0?width:height*Aspect(texture),height,1));
        const auto extent=(glm::abs(glm::vec3(model[0]))+glm::abs(glm::vec3(model[1])))*.5f+glm::vec3(.01f);
        instances.push_back({texture,model,base+glm::vec3(0,height*.5f,0),tint,extent});
        visible.reserve(instances.size());gpuInstances.reserve(instances.size());
        return instances.size()-1;
    }
    size_t Count() const {return instances.size();}
    const glm::mat4& Matrix(size_t id) const{return instances.at(id).model;}
    void Draw(const glm::mat4& view,const glm::mat4& projection) const
    {
        const auto clip=projection*view;const auto planes=ExtractFrustum(clip);
        visible.clear();gpuInstances.clear();
        for(size_t id=0;id<instances.size();++id)
        {
            const auto& instance=instances[id];
            if(BoundsVisible(planes,instance.center-instance.extent,instance.center+instance.extent))
                visible.push_back({id,(view*glm::vec4(instance.center,1)).z});
            else ++RenderMetrics::culled;
        }
        if(visible.empty())return;
        // Orden global de transparencia; agrupar SOLO runs contiguos de textura
        // conserva exactamente la mezcla de fachadas, cerca y arboles cruzados.
        std::sort(visible.begin(),visible.end(),[](const Visible& a,const Visible& b){return a.depth==b.depth?a.id<b.id:a.depth<b.depth;});
        for(const auto& item:visible)
        {const auto& instance=instances[item.id];gpuInstances.push_back({instance.model,glm::vec4(instance.tint,1)});}
        glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        instancedShader.Use();RenderState::BindVao(vao);
        glUniformMatrix4fv(instancedShader.Uniform("viewProjection"),1,GL_FALSE,&clip[0][0]);
        glBindBuffer(GL_ARRAY_BUFFER,instanceVbo);
        glBufferData(GL_ARRAY_BUFFER,instances.size()*sizeof(GpuInstance),nullptr,GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER,0,gpuInstances.size()*sizeof(GpuInstance),gpuInstances.data());
        for(size_t first=0;first<visible.size();)
        {
            const auto texture=instances[visible[first].id].texture;size_t end=first+1;
            while(end<visible.size()&&instances[visible[end].id].texture==texture)++end;
            SetInstanceAttributes(first*sizeof(GpuInstance));RenderState::BindTexture2D(textures[texture].id);
            glDrawElementsInstanced(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr,static_cast<GLsizei>(end-first));
            RenderMetrics::Draw(6,end-first);first=end;
        }
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    void ValidateAlpha() const
    {
        RenderState::Invalidate();
        // Prueba real del shader: alfa cero y alfa <0.1 dejan intacto el cielo;
        // alfa 128 mezcla rojo/azul al 50%, en lugar de volver el plano opaco.
        GLuint fbo=0,color=0,test=0;
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,12,4,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("FBO de transparencia incompleto.");
        const unsigned char pixels[]={255,255,255,0,255,255,255,20,255,0,0,128};
        glGenTextures(1,&test);glBindTexture(GL_TEXTURE_2D,test);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,3,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glViewport(0,0,12,4);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        shader.Use();glActiveTexture(GL_TEXTURE0);glBindVertexArray(vao);
        DrawQuad(test,glm::ortho(-.5f,.5f,0.0f,1.0f,-1.0f,1.0f),glm::vec3(1));
        unsigned char samples[3][4]{};
        for(int i=0;i<3;++i) glReadPixels(2+i*4,2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,samples[i]);
        // Mismos pixeles con el nuevo camino instanced: verifica layout de
        // atributos, tint, descarte y mezcla, no solamente el shader auxiliar.
        glClear(GL_COLOR_BUFFER_BIT);instancedShader.Use();RenderState::BindVao(vao);
        const auto clip=glm::ortho(-.5f,.5f,0.0f,1.0f,-1.0f,1.0f);
        const GpuInstance instance{glm::mat4(1),glm::vec4(1)};
        glUniformMatrix4fv(instancedShader.Uniform("viewProjection"),1,GL_FALSE,&clip[0][0]);
        glBindBuffer(GL_ARRAY_BUFFER,instanceVbo);glBufferData(GL_ARRAY_BUFFER,sizeof(instance),&instance,GL_STREAM_DRAW);
        SetInstanceAttributes(0);RenderState::BindTexture2D(test);
        glDrawElementsInstanced(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr,1);
        unsigned char instancedSamples[3][4]{};
        for(int i=0;i<3;++i)glReadPixels(2+i*4,2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,instancedSamples[i]);
        for(int i=0;i<3;++i)for(int c=0;c<4;++c)
            if(std::abs(int(instancedSamples[i][c])-int(samples[i][c]))>1)throw std::runtime_error("El lote instanced cambia la transparencia.");
        glBindVertexArray(0);glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteTextures(1,&test);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);
        glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glBindTexture(GL_TEXTURE_2D,0);
        for(int i=0;i<2;++i) if(samples[i][0]>1||samples[i][1]>1||samples[i][2]<254)
            throw std::runtime_error("El descarte alfa deja un recuadro visible.");
        if(std::abs(int(samples[2][0])-128)>1||samples[2][1]>1||std::abs(int(samples[2][2])-127)>1)
            throw std::runtime_error("El shader no conserva el alfa semitransparente.");
        std::cout<<"Transparencia GPU OK: descarte <0.1 y mezcla SRC_ALPHA real."<<std::endl;
        RenderState::Invalidate();
    }
private:
    Shader shader;
    Shader instancedShader;
    GLuint vao=0,vbo=0,ebo=0;
    GLuint instanceVbo=0;
    struct Visible{size_t id;float depth;};struct GpuInstance{glm::mat4 model;glm::vec4 tint;};
    mutable std::vector<Visible> visible;mutable std::vector<GpuInstance> gpuInstances;
    std::vector<Texture> textures;
    std::vector<Instance> instances;
    void SetInstanceAttributes(uintptr_t base) const
    {
        for(GLuint col=0;col<4;++col)
        {
            glEnableVertexAttribArray(3+col);glVertexAttribPointer(3+col,4,GL_FLOAT,GL_FALSE,sizeof(GpuInstance),reinterpret_cast<void*>(base+sizeof(glm::vec4)*col));
            glVertexAttribDivisor(3+col,1);
        }
        glEnableVertexAttribArray(7);glVertexAttribPointer(7,4,GL_FLOAT,GL_FALSE,sizeof(GpuInstance),reinterpret_cast<void*>(base+offsetof(GpuInstance,tint)));
        glVertexAttribDivisor(7,1);
    }
    void DrawQuad(GLuint texture,const glm::mat4& clip,const glm::vec3& tint) const
    {
        glBindTexture(GL_TEXTURE_2D,texture);
        glUniformMatrix4fv(shader.Uniform("clipFromLocal"),1,GL_FALSE,glm::value_ptr(clip));
        glUniform3fv(shader.Uniform("tint"),1,glm::value_ptr(tint));
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        RenderMetrics::Draw(6);
    }
};
