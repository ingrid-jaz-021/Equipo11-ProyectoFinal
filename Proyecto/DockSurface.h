#pragma once
#include <glm/glm.hpp>
#include "FloorUV.h"

namespace DockSurface
{
    inline bool IsDock(const glm::vec3& p){return p.x>0;}
    inline glm::vec3 AlignVertex(glm::vec3 p)
    {
        // Solo los dos bloques del muelle positivo en X. Se conserva la tarima
        // de la ofrenda occidental. El bloque estrecho tenia esquinas torcidas.
        if(!IsDock(p))return p;
        if(p.y>2||p.y<1)
        {
            p.x=p.x<39.1831f?5.188194f:73.178017f;
            p.z=p.z<-28.8963f?-62.891235f:5.098602f;
            p.y=p.y<1.65f?.820250f:2.491380f;
        }
        else
        {
            p.x=p.x<21.4389f?4.986202f:37.891533f;
            p.z=p.z<-29.2245f?-40.070999f:-18.377960f;
            p.y=p.y<1.38f?1.177705f:1.587894f;
        }
        return p;
    }
    inline glm::vec2 UV(const glm::vec3& p,const glm::vec3& normal)
    {return ProjectFloorUV(p-glm::vec3(5.188194f,0,-62.891235f),normal,{1.6f,3.2f});}
}
