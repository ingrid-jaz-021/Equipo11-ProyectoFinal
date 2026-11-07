#pragma once
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include "Model.h"
#include "Texture2D.h"

class TreeVegetation
{
    GLuint texture=0;
    std::unique_ptr<Mesh> quad;
    std::vector<glm::vec3> sites;
    float width=70.0f,height=70.0f;
public:
    TreeVegetation(const GroundSurface& ground,const glm::vec3& origin,float scale)
    {
        int w,h;
        texture=LoadRgbaTexture("images/Arbol_RGBA.png",GL_CLAMP_TO_EDGE,&w,&h);
        height=7.0f*scale;width=height*static_cast<float>(w)/h;
        const std::vector<Vertex> vertices={
            {{-0.5f,0,0},{0,0,1},{0,1}},{{0.5f,0,0},{0,0,1},{1,1}},
            {{0.5f,1,0},{0,0,1},{1,0}},{{-0.5f,1,0},{0,0,1},{0,0}}
        };
        MeshMaterial material;material.diffuseTexture=texture;
        quad=std::make_unique<Mesh>(vertices,std::vector<GLuint>{0,1,2,0,2,3},
            material,glm::vec3(-0.5f,0,0),glm::vec3(0.5f,1,0));
        std::ifstream input("Models/EntornoCompleto/Arboles.txt");
        if(!input) throw std::runtime_error("Faltan las posiciones de los arboles.");
        std::string line;
        while(std::getline(input,line))
        {
            if(line.empty()||line[0]=='#') continue;
            float bx,bz,x,z;std::istringstream values(line);
            if(!(values>>bx>>bz>>x>>z)) throw std::runtime_error("Posicion de arbol invalida.");
            const float distance=glm::length(glm::vec2(x-bx,z-bz));
            if(distance<2.5f||distance>6.0f) throw std::runtime_error("Arbol fuera del costado de su arbusto.");
            float y=origin.y;
            ground.Sample(x,z,origin.y+4.0f,y);
            sites.push_back((glm::vec3(x,y,z)-origin)*scale);
        }
        if(sites.size()!=8) throw std::runtime_error("Se esperan ocho arboles, uno por arbusto.");
    }
    ~TreeVegetation(){if(texture)glDeleteTextures(1,&texture);}
    TreeVegetation(const TreeVegetation&)=delete;
    TreeVegetation& operator=(const TreeVegetation&)=delete;
    size_t Count() const {return sites.size();}
    void Draw(const Shader& shader,const glm::mat4& view,const glm::mat4& projection) const
    {
        shader.Use();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program,"projection"),1,GL_FALSE,glm::value_ptr(projection));
        for(const auto& site:sites)
            for(int crossed=0;crossed<2;++crossed)
            {
                glm::mat4 model=glm::translate(glm::mat4(1),site);
                model=glm::rotate(model,glm::radians(90.0f*crossed),glm::vec3(0,1,0));
                model=glm::scale(model,glm::vec3(width,height,1));
                const glm::mat3 normal=glm::transpose(glm::inverse(glm::mat3(model)));
                glUniformMatrix4fv(glGetUniformLocation(shader.Program,"model"),1,GL_FALSE,glm::value_ptr(model));
                glUniformMatrix3fv(glGetUniformLocation(shader.Program,"normalMatrix"),1,GL_FALSE,glm::value_ptr(normal));
                quad->Draw(shader);
            }
        glBindVertexArray(0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);
    }
};
