#pragma once
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Model.h"

class WaterSurface
{
public:
    WaterSurface(const Model& environment,const glm::vec3& origin,float scale)
        :shader("Shader/water.vs","Shader/water.frag")
    {
        float minX,minZ,maxX,maxZ,tileWidth;
        std::ifstream input("Models/EntornoCompleto/Laguna/Agua/WaterPlane.txt");
        if(!(input>>minX>>minZ>>maxX>>maxZ>>tileWidth)||maxX<=minX||maxZ<=minZ||tileWidth<=0)
            throw std::runtime_error("Limites del plano de agua invalidos.");
        glm::vec3 deckLo,deckHi;
        if(!environment.GetMaterialBounds("Material.027",deckLo,deckHi))
            throw std::runtime_error("No se encontro la tarima de la Laguna para ajustar el agua.");
        const float level=deckLo.y-.40f;
        if(deckLo.x<minX||deckHi.x>maxX||deckLo.z<minZ||deckHi.z>maxZ)
            throw std::runtime_error("El plano de agua no cubre la Laguna.");
        int width,height;
        texture=LoadRgbaTexture("Models/EntornoCompleto/Laguna/Agua/River_Water_Texture-427.jpg",
            GL_MIRRORED_REPEAT,&width,&height);
        const float tileHeight=tileWidth*static_cast<float>(height)/width;
        const float u=(maxX-minX)/tileWidth,v=(maxZ-minZ)/tileHeight;
        const std::vector<Vertex> vertices={
            {{minX,level,minZ},{0,1,0},{0,0}},{{maxX,level,minZ},{0,1,0},{u,0}},
            {{maxX,level,maxZ},{0,1,0},{u,v}},{{minX,level,maxZ},{0,1,0},{0,v}}
        };
        MeshMaterial material;material.diffuseTexture=texture;
        quad=std::make_unique<Mesh>(vertices,std::vector<GLuint>{0,2,1,0,3,2},material,
            glm::vec3(minX,level,minZ),glm::vec3(maxX,level,maxZ));
        model=glm::scale(glm::translate(glm::mat4(1),-origin*scale),glm::vec3(scale));
        center=(glm::vec3((minX+maxX)*.5f,level,(minZ+maxZ)*.5f)-origin)*scale;
        tileWorld=glm::vec2(tileWidth,tileHeight)*scale;
        shader.Use();glUniform1i(glGetUniformLocation(shader.Program,"texture_diffuse1"),0);
        std::cout<<"Agua Laguna: plano "<<maxX-minX<<" x "<<maxZ-minZ<<", Y="<<level
            <<", UV "<<u<<" x "<<v<<"; debajo de la tarima y con UV scrolling."<<std::endl;
    }
    ~WaterSurface(){if(texture)glDeleteTextures(1,&texture);}
    WaterSurface(const WaterSurface&)=delete;
    WaterSurface& operator=(const WaterSurface&)=delete;
    const glm::vec3& Center() const{return center;}
    void Draw(const glm::mat4& view,const glm::mat4& projection,const glm::vec3& eye,double seconds) const
    {
        shader.Use();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shader.Program,"eyePosition"),1,glm::value_ptr(eye));
        glUniform1f(glGetUniformLocation(shader.Program,"time"),static_cast<float>(seconds));
        quad->Draw(shader);glBindVertexArray(0);glBindTexture(GL_TEXTURE_2D,0);
    }
    void ValidateAnimation() const
    {
        GLuint fbo=0,color=0;
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("FBO de agua incompleto.");
        const auto eye=center+glm::vec3(0,100,0);
        const auto view=glm::lookAt(eye,center,glm::vec3(0,0,-1));
        const auto projection=glm::ortho(-tileWorld.x*.5f,tileWorld.x*.5f,-tileWorld.y*.5f,tileWorld.y*.5f,1.0f,200.0f);
        std::vector<unsigned char> first(32*32*4),second(first.size());
        glViewport(0,0,32,32);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
        for(int i=0;i<2;++i)
        {
            glClear(GL_COLOR_BUFFER_BIT);Draw(view,projection,eye,i*4.0);
            glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,i?second.data():first.data());
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);glEnable(GL_DEPTH_TEST);
        unsigned difference=0;
        for(size_t i=0;i<first.size();++i)if(i%4!=3)difference+=std::abs(int(first[i])-int(second[i]));
        if(difference<128)throw std::runtime_error("El shader de agua no anima sus UV con el tiempo.");
        std::cout<<"Agua GPU OK: dos tiempos producen imagenes distintas (diferencia "<<difference<<")."<<std::endl;
    }
private:
    Shader shader;GLuint texture=0;std::unique_ptr<Mesh> quad;
    glm::mat4 model=glm::mat4(1);glm::vec3 center=glm::vec3(0);glm::vec2 tileWorld=glm::vec2(1);
};
