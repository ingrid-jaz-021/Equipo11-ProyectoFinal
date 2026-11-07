#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.h"
#include "Texture2D.h"

class AlphaBillboards
{
public:
    struct Texture {GLuint id;int width,height;};
    struct Instance {size_t texture;glm::mat4 model;glm::vec3 center,tint;};
    AlphaBillboards():shader("Shader/billboard.vs","Shader/billboard.frag")
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
        shader.Use();glUniform1i(glGetUniformLocation(shader.Program,"texture_diffuse1"),0);
    }
    ~AlphaBillboards()
    {
        for(const auto& texture:textures) glDeleteTextures(1,&texture.id);
        glDeleteBuffers(1,&ebo);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);
    }
    AlphaBillboards(const AlphaBillboards&)=delete;
    AlphaBillboards& operator=(const AlphaBillboards&)=delete;
    size_t Load(const std::string& file)
    {
        Texture t{};t.id=LoadRgbaTexture(file,GL_CLAMP_TO_EDGE,&t.width,&t.height,true);
        textures.push_back(t);return textures.size()-1;
    }
    float Aspect(size_t texture) const {const auto& t=textures.at(texture);return static_cast<float>(t.width)/t.height;}
    void Add(size_t texture,const glm::vec3& base,float yaw,float height,const glm::vec3& tint=glm::vec3(.82f,.83f,.87f))
    {
        glm::mat4 model=glm::translate(glm::mat4(1),base);
        model=glm::rotate(model,yaw,glm::vec3(0,1,0));
        model=glm::scale(model,glm::vec3(height*Aspect(texture),height,1));
        instances.push_back({texture,model,base+glm::vec3(0,height*.5f,0),tint});
    }
    size_t Count() const {return instances.size();}
    void Draw(const glm::mat4& view,const glm::mat4& projection) const
    {
        // Un unico orden para fachadas y ambos quads de cada arbol.
        std::vector<size_t> order(instances.size());std::iota(order.begin(),order.end(),0);
        std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){
            return (view*glm::vec4(instances[a].center,1)).z<(view*glm::vec4(instances[b].center,1)).z;});
        glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        shader.Use();glActiveTexture(GL_TEXTURE0);glBindVertexArray(vao);
        const glm::mat4 clip=projection*view;
        for(size_t id:order)
        {
            const auto& instance=instances[id];
            DrawQuad(textures[instance.texture].id,clip*instance.model,instance.tint);
        }
        glBindVertexArray(0);glBindTexture(GL_TEXTURE_2D,0);
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    void ValidateAlpha() const
    {
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
        glBindVertexArray(0);glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteTextures(1,&test);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);
        glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glBindTexture(GL_TEXTURE_2D,0);
        for(int i=0;i<2;++i) if(samples[i][0]>1||samples[i][1]>1||samples[i][2]<254)
            throw std::runtime_error("El descarte alfa deja un recuadro visible.");
        if(std::abs(int(samples[2][0])-128)>1||samples[2][1]>1||std::abs(int(samples[2][2])-127)>1)
            throw std::runtime_error("El shader no conserva el alfa semitransparente.");
        std::cout<<"Transparencia GPU OK: descarte <0.1 y mezcla SRC_ALPHA real."<<std::endl;
    }
private:
    Shader shader;
    GLuint vao=0,vbo=0,ebo=0;
    std::vector<Texture> textures;
    std::vector<Instance> instances;
    void DrawQuad(GLuint texture,const glm::mat4& clip,const glm::vec3& tint) const
    {
        glBindTexture(GL_TEXTURE_2D,texture);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program,"clipFromLocal"),1,GL_FALSE,glm::value_ptr(clip));
        glUniform3fv(glGetUniformLocation(shader.Program,"tint"),1,glm::value_ptr(tint));
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
    }
};
