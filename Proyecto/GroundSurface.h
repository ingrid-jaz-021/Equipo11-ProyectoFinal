#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

class GroundSurface
{
public:
    static bool IsRaisedGrass(const std::string& material)
    {
        return material=="Material.008"||material=="Material.009"||material=="Material.010"||
            material=="Material.011"||material=="Material.012"||material=="Material.015"||
            material=="Material.021"||material=="Material.022";
    }
    void Add(const glm::vec3& a,const glm::vec3& b,const glm::vec3& c,bool grass=false)
    {
        const glm::vec3 normal=glm::cross(b-a,c-a);
        const float area2=glm::length(normal);
        // Las caras de pasto pueden venir con winding invertido; ambas tapas
        // se aceptan y el raycast elige la mas alta, nunca una pared vertical.
        const float up=grass?std::abs(normal.y):normal.y;
        if(area2<0.04f||up<area2*0.60f) return;
        const auto lo=glm::min(a,glm::min(b,c)),hi=glm::max(a,glm::max(b,c));
        const unsigned id=static_cast<unsigned>(triangles.size());
        triangles.push_back({a,b,c,grass});
        for(int z=Cell(lo.z);z<=Cell(hi.z);++z)
            for(int x=Cell(lo.x);x<=Cell(hi.x);++x) grid[Key(x,z)].push_back(id);
    }
    bool Sample(float x,float z,float currentY,float& height) const
    {
        return Query(x,z,currentY,currentY,false,height);
    }
    bool SampleCharacter(float x,float z,float currentY,float baseFloor,float& height,float radius=0.0f) const
    {
        // Rayos verticales bajo el centro y cuatro extremos de la huella.
        // Soporte parcial sobre la jardinera mantiene ambos pies por encima.
        const std::array<glm::vec2,5> points={glm::vec2(x,z),glm::vec2(x+radius,z),
            glm::vec2(x-radius,z),glm::vec2(x,z+radius),glm::vec2(x,z-radius)};
        bool found=false;float highest=-(std::numeric_limits<float>::max)();
        for(size_t i=0;i<(radius>0?points.size():1);++i)
        {
            float y=0;
            if(Query(points[i].x,points[i].y,currentY,baseFloor,true,y))
            {
                highest=std::max(highest,y);found=true;
            }
        }
        if(found) height=highest;
        return found;
    }
    std::vector<glm::vec3> RaisedSamples(float floor) const
    {
        std::vector<glm::vec3> samples;
        for(const auto& t:triangles)
        {
            const glm::vec3 center=(t.a+t.b+t.c)/3.0f;
            if(t.grass&&center.y>floor+0.5f) samples.push_back(center);
        }
        return samples;
    }
private:
    struct Triangle {glm::vec3 a,b,c;bool grass;};
    std::vector<Triangle> triangles;
    std::unordered_map<uint64_t,std::vector<unsigned>> grid;
    bool Query(float x,float z,float currentY,float baseFloor,bool climbGrass,float& height) const
    {
        auto cell=grid.find(Key(Cell(x),Cell(z)));
        if(cell==grid.end()) return false;
        bool found=false;float highest=-1.0e20f;
        for(unsigned id:cell->second)
        {
            const auto& t=triangles[id];
            const float det=(t.b.z-t.c.z)*(t.a.x-t.c.x)+(t.c.x-t.b.x)*(t.a.z-t.c.z);
            if(std::abs(det)<1e-8f) continue;
            const float u=((t.b.z-t.c.z)*(x-t.c.x)+(t.c.x-t.b.x)*(z-t.c.z))/det;
            const float v=((t.c.z-t.a.z)*(x-t.c.x)+(t.a.x-t.c.x)*(z-t.c.z))/det;
            if(u<-0.0001f||v<-0.0001f||u+v>1.0001f) continue;
            const float y=u*t.a.y+v*t.b.y+(1-u-v)*t.c.y;
            const float ceiling=climbGrass&&t.grass?baseFloor+6.0f:currentY+0.40f;
            if(y<=ceiling&&y>=std::min(currentY,baseFloor)-8.0f&&y>highest)
            {
                highest=y;found=true;
            }
        }
        if(found) height=highest;
        return found;
    }
    static int Cell(float value){return static_cast<int>(std::floor(value/8.0f));}
    static uint64_t Key(int x,int z)
    {
        return(static_cast<uint64_t>(static_cast<uint32_t>(x))<<32)|static_cast<uint32_t>(z);
    }
};
