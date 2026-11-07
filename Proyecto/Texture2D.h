#pragma once
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <GL/glew.h>
#include "UnicodeIO.h"
#include "stb_image.h"
#include "CutoutImage.h"

inline GLuint LoadRgbaTexture(const std::string& filename, GLenum wrap=GL_REPEAT, int* outWidth=nullptr, int* outHeight=nullptr,
    bool cleanCutout=false)
{
    FILE* file=OpenUtf8File(filename.c_str(),L"rb");
    if(!file) throw std::runtime_error("No se pudo abrir la textura: "+filename);
    int width=0,height=0,channels=0;
    unsigned char* image=stbi_load_from_file(file,&width,&height,&channels,STBI_rgb_alpha);
    std::fclose(file);
    if(!image) throw std::runtime_error("No se pudo decodificar la textura: "+filename);
    std::vector<unsigned char> cutout;
    if(cleanCutout)
    {
        cutout.assign(image,image+static_cast<size_t>(width)*height*4);
        stbi_image_free(image);image=nullptr;
        PrepareCutoutImage(cutout,width,height);
    }
    GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    GLint alignment;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,
        cleanCutout?cutout.data():image);
    stbi_image_free(image);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
    if(GLEW_EXT_texture_filter_anisotropic)
    {
        GLfloat maximum=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);
        const float level=std::min(16.0f,maximum);
        glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,level);
        static bool announced=false;
        if(!announced){std::cout<<"Filtrado anisotropico: "<<level<<"x."<<std::endl;announced=true;}
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
    glBindTexture(GL_TEXTURE_2D,0);
    if(outWidth) *outWidth=width;if(outHeight) *outHeight=height;
    return texture;
}
