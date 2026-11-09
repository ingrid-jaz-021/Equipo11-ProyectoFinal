#pragma once
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <GL/glew.h>
#include "UnicodeIO.h"
#include "stb_image.h"
#include "CutoutImage.h"

// Todos los origenes (archivo, glTF/FBX incrustado o aiTexel) comparten el mismo filtrado.
inline GLuint UploadRgbaTexture(const unsigned char* pixels,int width,int height,GLenum wrap=GL_REPEAT,bool cleanCutout=false)
{
    if(!pixels||width<=0||height<=0)throw std::runtime_error("Textura RGBA vacia.");
    std::vector<unsigned char> cutout;
    if(cleanCutout)
    {
        cutout.assign(pixels,pixels+static_cast<size_t>(width)*height*4);
        PrepareCutoutImage(cutout,width,height);
    }
    GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    GLint alignment;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,
        cleanCutout?cutout.data():pixels);
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
    return texture;
}
inline GLuint DecodeRgbaTexture(const unsigned char* data,size_t size,GLenum wrap=GL_REPEAT,int* outWidth=nullptr,int* outHeight=nullptr)
{
    if(!data||size==0||size>static_cast<size_t>((std::numeric_limits<int>::max)()))throw std::runtime_error("Imagen incrustada invalida.");
    int width=0,height=0,channels=0;
    std::unique_ptr<unsigned char,decltype(&stbi_image_free)> pixels(stbi_load_from_memory(data,static_cast<int>(size),&width,&height,&channels,STBI_rgb_alpha),stbi_image_free);
    if(!pixels)throw std::runtime_error("No se pudo decodificar PNG/JPEG incrustado.");
    const GLuint texture=UploadRgbaTexture(pixels.get(),width,height,wrap);
    if(outWidth)*outWidth=width;if(outHeight)*outHeight=height;return texture;
}
inline GLuint LoadRgbaTexture(const std::string& filename,GLenum wrap=GL_REPEAT,int* outWidth=nullptr,int* outHeight=nullptr,bool cleanCutout=false)
{
    FILE* file=OpenUtf8File(filename.c_str(),L"rb");
    if(!file)throw std::runtime_error("No se pudo abrir la textura: "+filename);
    int width=0,height=0,channels=0;
    std::unique_ptr<unsigned char,decltype(&stbi_image_free)> pixels(stbi_load_from_file(file,&width,&height,&channels,STBI_rgb_alpha),stbi_image_free);
    std::fclose(file);
    if(!pixels)throw std::runtime_error("No se pudo decodificar la textura: "+filename);
    const GLuint texture=UploadRgbaTexture(pixels.get(),width,height,wrap,cleanCutout);
    if(outWidth)*outWidth=width;if(outHeight)*outHeight=height;return texture;
}
