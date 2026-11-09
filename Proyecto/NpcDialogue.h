#pragma once
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Texture2D.h"
#include "RenderMetrics.h"

// Texto estatico rasterizado con Segoe UI y Unicode; un solo quad por cuadro.
// La frase exacta se mantiene en tools/build_npc_dialogue.ps1, sin dependencia
// de fuentes o librerias de texto al ejecutar el juego.
class NpcDialogue
{
public:
    explicit NpcDialogue(const std::vector<std::string>& images={"images/IvanovichDialogo.png"}):shader("Shader/dialogue.vs","Shader/dialogue.frag")
    {
        for(const auto& image:images)textures.push_back(LoadRgbaTexture(image,GL_CLAMP_TO_EDGE,&textureWidth,&textureHeight));
        if(textures.empty())throw std::runtime_error("Dialogo sin imagenes.");
        const float vertices[]={0,0,0,1, 1,0,1,1, 1,1,1,0, 0,1,0,0};
        const GLuint indices[]={0,1,2,0,2,3};
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ebo);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));
        glBindVertexArray(0);shader.Use();glUniform1i(shader.Uniform("dialogueTexture"),0);
    }
    ~NpcDialogue()
    {glDeleteTextures(static_cast<GLsizei>(textures.size()),textures.data());glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ebo);glDeleteVertexArrays(1,&vao);}
    NpcDialogue(const NpcDialogue&)=delete;NpcDialogue& operator=(const NpcDialogue&)=delete;
    static bool IsNearby(const glm::vec3& girl,const glm::vec3& npc,float scale)
    {
        const auto delta=girl-npc;
        return glm::dot(glm::vec2(delta.x,delta.z),glm::vec2(delta.x,delta.z))<=4.5f*scale*4.5f*scale&&std::abs(delta.y)<=3.0f*scale;
    }
    void Draw(bool visible,int width,int height) const
    {
        if(visible)DrawPanel(0,width,height);
    }
    void DrawPanel(size_t page,int width,int height) const
    {
        if(page>=textures.size()||width<=0||height<=0)return;
        const float uiScale=glm::clamp(height/720.0f,.7f,2.0f);
        const float panelWidth=std::min(width*.90f,1000.0f*uiScale),panelHeight=panelWidth*textureHeight/textureWidth;
        shader.Use();
        glUniform2f(shader.Uniform("viewportSize"),static_cast<float>(width),static_cast<float>(height));
        glUniform4f(shader.Uniform("screenRect"),(width-panelWidth)*.5f,std::max(18.0f,24*uiScale),panelWidth,panelHeight);
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        RenderState::BindVao(vao);RenderState::BindTexture2D(textures[page]);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);RenderMetrics::Draw(6);
        glDisable(GL_BLEND);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);
    }
private:
    Shader shader;std::vector<GLuint> textures;GLuint vao=0,vbo=0,ebo=0;int textureWidth=1,textureHeight=1;
};
