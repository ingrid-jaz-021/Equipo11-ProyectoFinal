#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
#include "AlphaBillboards.h"
#include "WaterSurface.h"

// Cinco tramos continuos: cuatro caras, con la cara oeste dividida por el muelle.
// Comparte quad, shader RGBA y orden de transparencias con arboles/fachadas.
class LakeFence
{
public:
    LakeFence(AlphaBillboards& planes,const WaterSurface& water,const glm::vec3& origin,float scale)
        :origin(origin),scale(scale)
    {
        const auto lo=water.SourceMin(),hi=water.SourceMax();
        bounds=glm::vec4(lo.x-.15f,lo.z-.15f,hi.x+.15f,hi.z+.15f);
        baseY=lo.y-.05f;
        const auto dockLo=water.DockMin(),dockHi=water.DockMax();
        if(std::abs(dockLo.x-lo.x)>.1f)
            throw std::runtime_error("El muelle ya no intersecta la orilla oeste del lago.");
        // Reservar TODO el ancho de la plataforma, no solo el centro del sendero.
        opening=glm::vec2(dockLo.z-1.5f,dockHi.z+1.5f);
        if(opening.x<=bounds.y||opening.y>=bounds.w)
            throw std::runtime_error("La entrada del lago no cabe en su perimetro.");
        const size_t texture=planes.Load("images/Cerca_lago.png");
        const float nominalWidth=Height*planes.Aspect(texture);
        const std::array<Span,5> spans={
            Span{0,bounds.x,bounds.z},Span{1,bounds.y,bounds.w},Span{2,bounds.x,bounds.z},
            Span{3,bounds.y,opening.x},Span{3,opening.y,bounds.w}
        };
        for(const auto& span:spans)
        {
            const int count=std::max(1,static_cast<int>(std::round((span.end-span.begin)/nominalWidth)));
            const float step=(span.end-span.begin)/count;
            for(int i=0;i<count;++i)
            {
                // Solo solapar juntas interiores; jamas extenderse hacia el hueco.
                const float a=span.begin+i*step-(i>0?.005f:0);
                const float b=span.begin+(i+1)*step+(i+1<count?.005f:0);
                const float middle=(a+b)*.5f;
                glm::vec3 site;
                if(span.side==0)site={middle,baseY,bounds.y};
                else if(span.side==1)site={bounds.z,baseY,middle};
                else if(span.side==2)site={middle,baseY,bounds.w};
                else site={bounds.x,baseY,middle};
                const float yaw=span.side==0?0:span.side==1?-HalfPi:span.side==2?2*HalfPi:HalfPi;
                const size_t id=planes.Add(texture,(site-origin)*scale,yaw,Height*scale,
                    glm::vec3(.82f,.83f,.87f),(b-a)*scale);
                panels.push_back({span.side,a,b,id});
            }
        }
        Validate(planes,spans);
        std::cout<<"Cerca del lago: "<<panels.size()<<" planos RGBA, perimetro X "<<bounds.x<<".."<<bounds.z
            <<", Z "<<bounds.y<<".."<<bounds.w<<"; entrada oeste libre Z "<<opening.x<<".."<<opening.y<<"."<<std::endl;
    }
    glm::vec3 EntryTarget() const{return World({bounds.x,baseY+Height*.6f,(opening.x+opening.y)*.5f});}
    glm::vec3 EntryEye(bool inside) const
    {return World({bounds.x+(inside?65.0f:-40.0f),baseY+14,(opening.x+opening.y)*.5f+15});}
    glm::vec3 RailTarget() const{return World({bounds.z,baseY+Height*.5f,(bounds.y+bounds.w)*.5f});}
    glm::vec3 RailEye() const{return RailTarget()+glm::vec3(-12,2,8)*scale;}
private:
    struct Span{int side;float begin,end;};
    struct Panel{int side;float begin,end;size_t id;};
    static constexpr float Height=3.5f,HalfPi=1.57079632679f;
    glm::vec4 bounds=glm::vec4(0);glm::vec2 opening=glm::vec2(0);
    glm::vec3 origin;float scale,baseY=0;
    std::vector<Panel> panels;
    glm::vec3 World(const glm::vec3& p) const{return (p-origin)*scale;}
    void Validate(const AlphaBillboards& planes,const std::array<Span,5>& spans) const
    {
        for(const auto& panel:panels)
        {
            const auto& matrix=planes.Matrix(panel.id);
            const auto a=glm::vec3(matrix*glm::vec4(-.5f,0,0,1))/scale+origin;
            const auto b=glm::vec3(matrix*glm::vec4(.5f,0,0,1))/scale+origin;
            const bool alongX=panel.side==0||panel.side==2;
            const float lo=std::min(alongX?a.x:a.z,alongX?b.x:b.z),hi=std::max(alongX?a.x:a.z,alongX?b.x:b.z);
            const float fixed=panel.side==0?bounds.y:panel.side==1?bounds.z:panel.side==2?bounds.w:bounds.x;
            if(std::abs(lo-panel.begin)>.001f||std::abs(hi-panel.end)>.001f||
                std::abs((alongX?a.z:a.x)-fixed)>.001f||std::abs((alongX?b.z:b.x)-fixed)>.001f||
                std::abs(a.y-baseY)>.001f||std::abs(glm::length(glm::vec3(matrix[1]))/scale-Height)>.001f)
                throw std::runtime_error("Un quad de cerca no coincide con su tramo real.");
            if(panel.side==3&&panel.end>opening.x+.001f&&panel.begin<opening.y-.001f)
                throw std::runtime_error("La cerca bloquea el muelle de entrada.");
        }
        for(const auto& span:spans)
        {
            float covered=span.begin;
            for(const auto& panel:panels)if(panel.side==span.side&&panel.begin>=span.begin-.001f&&panel.end<=span.end+.001f)
            {
                if(panel.begin>covered+.001f)throw std::runtime_error("Hay un hueco entre paneles de la cerca.");
                covered=std::max(covered,panel.end);
            }
            if(std::abs(covered-span.end)>.001f)throw std::runtime_error("La cerca no cierra una cara del lago.");
        }
        // El corredor completo, incluida la huella lateral de la nina, queda libre.
        const float radius=MovementSettings::CharacterHeight*.12f;
        for(float z=opening.x+radius;z<=opening.y-radius;z+=radius)
            for(const auto& panel:panels)if(panel.side==3&&z+radius>panel.begin&&z-radius<panel.end)
                throw std::runtime_error("La huella del personaje invade la cerca al entrar al lago.");
    }
};
