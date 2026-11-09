#pragma once
#include <cmath>
#include <memory>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Mesh.h"

class MoonSky
{
public:
    explicit MoonSky(float scale):shader("Shader/moon.vs","Shader/moon.frag"),radius(28*scale)
    {
        focus={160*scale,0,-160*scale};position=focus+glm::vec3(150*scale,1000*scale,-100*scale);
        toMoon=glm::normalize(position-focus);
        constexpr int rows=32,columns=64;constexpr float pi=3.14159265359f;
        std::vector<Vertex> vertices;std::vector<GLuint> indices;
        for(int row=0;row<=rows;++row)for(int col=0;col<=columns;++col)
        {
            const float v=static_cast<float>(row)/rows,u=static_cast<float>(col)/columns;
            const float latitude=(v-.5f)*pi,longitude=u*pi*2;
            const glm::vec3 p(std::cos(latitude)*std::cos(longitude),std::sin(latitude),std::cos(latitude)*std::sin(longitude));
            vertices.push_back({p,p,{u,v}});
        }
        for(int row=0;row<rows;++row)for(int col=0;col<columns;++col)
        {
            const GLuint a=row*(columns+1)+col,b=a+columns+1;
            if(row>0)indices.insert(indices.end(),{a,b,a+1});
            if(row<rows-1)indices.insert(indices.end(),{a+1,b,b+1});
        }
        sphere=std::make_unique<Mesh>(vertices,indices,MeshMaterial{},glm::vec3(-1),glm::vec3(1));
        model=glm::scale(glm::translate(glm::mat4(1),position),glm::vec3(radius));
        std::cout<<"Luna: esfera procedural en Y="<<position.y<<", radio="<<radius<<"; luz direccional azulada en toda la escena."<<std::endl;
    }
    const glm::vec3& Position() const{return position;}
    const glm::vec3& Focus() const{return focus;}
    const glm::vec3& ToMoon() const{return toMoon;}
    glm::vec3 Ambient() const{return {.17f,.20f,.28f};}
    glm::vec3 Diffuse() const{return {.32f,.36f,.47f};}
    float FarPlane(const glm::vec3& eye) const{return glm::length(position-eye)+radius*2;}
    void UploadLighting(const Shader& target) const
    {
        target.Use();const auto incoming=-toMoon,ambient=Ambient(),diffuse=Diffuse();
        glUniform3fv(target.Uniform("lightDirection"),1,glm::value_ptr(incoming));
        glUniform3fv(target.Uniform("ambientLight"),1,glm::value_ptr(ambient));
        glUniform3fv(target.Uniform("diffuseLight"),1,glm::value_ptr(diffuse));
    }
    void Draw(const glm::mat4& view,const glm::mat4& projection,const glm::vec3& eye) const
    {
        if(!TransformedBoundsVisible(ExtractFrustum(projection*view),glm::vec3(-1),glm::vec3(1),model)){++RenderMetrics::culled;return;}
        shader.Use();glUniformMatrix4fv(shader.Uniform("model"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(shader.Uniform("view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(shader.Uniform("projection"),1,GL_FALSE,glm::value_ptr(projection));
        const auto viewer=glm::normalize(eye-position);glUniform3fv(shader.Uniform("viewerDirection"),1,glm::value_ptr(viewer));
        sphere->Draw(shader);
    }
    void Validate(const Shader& environmentShader,const Shader& characterShader,const Shader& lampShader) const
    {
        for(const Shader* material:{&environmentShader,&characterShader,&lampShader})
        {
            glm::vec3 direction(0),ambient(0),diffuse(0);
            glGetUniformfv(material->Program,material->Uniform("lightDirection"),glm::value_ptr(direction));
            glGetUniformfv(material->Program,material->Uniform("ambientLight"),glm::value_ptr(ambient));
            glGetUniformfv(material->Program,material->Uniform("diffuseLight"),glm::value_ptr(diffuse));
            if(glm::length(direction+toMoon)>.001f||ambient.b<=ambient.r||diffuse.b<=diffuse.r)throw std::runtime_error("La iluminacion no proviene de la Luna.");
        }
        RenderState::Invalidate();GLuint fbo=0,color=0;
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("FBO lunar incompleto.");
        glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
        const auto view=glm::lookAt(focus,position,glm::vec3(0,1,0));
        const auto projection=glm::ortho(-radius*1.1f,radius*1.1f,-radius*1.1f,radius*1.1f,1.0f,FarPlane(focus));
        Draw(view,projection,focus);std::vector<unsigned char> pixels(64*64*4);
        glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        size_t lit=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]>90&&pixels[i+2]>90)++lit;
        glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);glEnable(GL_DEPTH_TEST);RenderState::Invalidate();
        if(lit<1500||lit>3300)throw std::runtime_error("La esfera lunar no se ve correctamente.");
        std::cout<<"Luna GPU OK: esfera visible, crateres emisivos y luz fria compartida por entorno, personajes y farolas."<<std::endl;
    }
private:
    Shader shader;std::unique_ptr<Mesh> sphere;glm::vec3 focus,position,toMoon;glm::mat4 model=glm::mat4(1);float radius;
};
