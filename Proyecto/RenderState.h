#pragma once
#include <GL/glew.h>

// El cache solo cubre el pase de dibujo. Se invalida al comenzar cada frame,
// despues de cargas/tests que pueden tocar OpenGL directamente.
namespace RenderState
{
    inline GLuint program=~0u,texture=~0u,vao=~0u;
    inline bool textureUnitKnown=false;
    inline const void* material=nullptr;
    inline void Invalidate(){program=texture=vao=~0u;textureUnitKnown=false;material=nullptr;}
    inline void UseProgram(GLuint value)
    {if(program!=value){glUseProgram(value);program=value;material=nullptr;}}
    inline void BindTexture2D(GLuint value)
    {
        if(!textureUnitKnown){glActiveTexture(GL_TEXTURE0);textureUnitKnown=true;}
        if(texture!=value){glBindTexture(GL_TEXTURE_2D,value);texture=value;}
    }
    inline void BindVao(GLuint value){if(vao!=value){glBindVertexArray(value);vao=value;}}
}
