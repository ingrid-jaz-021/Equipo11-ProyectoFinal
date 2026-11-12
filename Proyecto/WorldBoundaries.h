#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "SolidObstacles.h"

// Invisible collision only. All geometry/UVs, fly-camera and boat controls stay separate.
class WorldBoundaries
{
public:
    using Rect=GroundSurface::Footprint;
    void Build(const Model& scene,GroundSurface& terrain,const glm::vec3& sourceOrigin,float worldScale,
        const glm::vec3& lakeMin,const glm::vec3& lakeMax)
    {
        origin=sourceOrigin;scale=worldScale;safePositions.clear();
        glm::vec3 plazaLo,plazaHi,entryLo,entryHi;
        if(!scene.GetMaterialBounds("Material.013",plazaLo,plazaHi)||
            !scene.GetMaterialEndBounds("TapeteAserrin1.002",2,1,entryLo,entryHi))
            throw std::runtime_error("No se pueden medir los limites Zocalo/Pasillo.");
        for(const char* name:{"Material.016","Material.017","Material.018","Material.019","Material.020"})
        {
            glm::vec3 lo,hi;
            if(!scene.GetMaterialBounds(name,lo,hi))throw std::runtime_error("Falta el borde de la plaza.");
            plazaLo=glm::min(plazaLo,lo);plazaHi=glm::max(plazaHi,hi);
        }
        plaza={{plazaLo.x,plazaLo.z},{plazaHi.x,plazaHi.z}};
        auto floors=terrain.FloorFootprints(origin.y+2.1f);
        const float center=(entryLo.x+entryHi.x)*.5f;
        float fromZ=entryHi.z,fromX=entryLo.x,toX=entryHi.x;
        // Extend to the real pavement, not just the narrower decorative carpet.
        for(const auto& r:floors)if(r.min.x<center&&r.max.x>center&&r.min.y<entryHi.z&&
            r.max.y>=entryHi.z-.05f&&r.max.y<plaza.min.y+.05f&&r.max.x-r.min.x<64)
        {
            if(r.max.y>fromZ+.01f){fromX=r.min.x;toX=r.max.x;fromZ=r.max.y;}
            else if(std::abs(r.max.y-fromZ)<.01f){fromX=std::min(fromX,r.min.x);toX=std::max(toX,r.max.x);}
        }
        AddEntranceBridge(terrain,{{fromX,fromZ-.25f},{toX,plaza.min.y+.25f}},center,origin.y);
        areas=terrain.FloorFootprints(origin.y+2.1f);
        areas.push_back(plaza);
        // BoatNavigation adds a 4-unit shore. Keep its walkways/boarding portal open;
        // exact ground support still prevents walking on the water itself.
        areas.push_back({{lakeMin.x-4,lakeMin.z-4},{lakeMax.x+4,lakeMax.z+4}});
        FinishWalls();
        std::cout<<"Limites del mundo: "<<wallBoxes.size()<<" AABB exteriores y puente continuo Zocalo/Pasillo."<<std::endl;
    }

    // Shared by the real scene and the isolated physics validation.
    void AddEntranceBridge(GroundSurface& terrain,const Rect& span,float centerX,float referenceY)
    {
        if(span.max.x<=span.min.x||span.max.y<=span.min.y||span.max.y-span.min.y>12)
            throw std::runtime_error("Conexion Zocalo/Pasillo invalida.");
        float south=referenceY,north=referenceY;
        if(!terrain.SampleCharacter(centerX,span.min.y,referenceY,south,0,3)||
            !terrain.SampleCharacter(centerX,span.max.y,referenceY,north,0,3))
            throw std::runtime_error("El puente debe conectar dos superficies reales.");
        const glm::vec3 a(span.min.x,south,span.min.y),b(span.max.x,south,span.min.y);
        const glm::vec3 c(span.max.x,north,span.max.y),d(span.min.x,north,span.max.y);
        terrain.Add(a,c,b);terrain.Add(a,d,c);entrance=span;entranceFloor=std::min(south,north);
    }

    void ConfigureForGround(const GroundSurface& terrain,const glm::vec3& sourceOrigin,float worldScale)
    {
        origin=sourceOrigin;scale=worldScale;safePositions.clear();
        areas=terrain.FloorFootprints(origin.y+2.1f);FinishWalls();
    }

    void Resolve(CharacterController& actor,const glm::vec3& before,const GroundSurface& terrain) const
    {
        if(actor.IsFrozen())return;
        float floor=0;
        if(!Supported(actor,before,before.y,terrain,floor))
        {
            auto found=safePositions.find(&actor);
            glm::vec3 safe=found==safePositions.end()?glm::vec3(0):found->second;
            if(!Supported(actor,safe,safe.y,terrain,floor))
                throw std::runtime_error("No hay un punto seguro para recuperar al personaje.");
            actor.position=safe;actor.position.y=(floor-origin.y)*scale;actor.velocity=glm::vec3(0);
            actor.ConformToGround(terrain,origin,scale,0);return;
        }
        safePositions[&actor]=before;
        walls.Resolve(actor,before);
        const glm::vec3 proposed=actor.position;
        glm::vec3 result=SweepSupport(actor,before,proposed,terrain);
        if(glm::length(glm::vec2(result.x-proposed.x,result.z-proposed.z))>.0001f)
        {
            // Slide along an edge instead of cancelling both movement axes.
            for(int axis:{0,2})
            {
                auto along=result;along[axis]=proposed[axis];
                result=SweepSupport(actor,result,along,terrain);
            }
            if(std::abs(result.x-proposed.x)>.0001f)actor.velocity.x=0;
            if(std::abs(result.z-proposed.z)>.0001f)actor.velocity.z=0;
        }
        actor.position.x=result.x;actor.position.z=result.z;
        // A pet may already have integrated gravity before its swept correction.
        // Only repair penetration; normal descending/grass/ramp motion stays intact.
        if(Supported(actor,actor.position,before.y,terrain,floor)&&actor.position.y<(floor-origin.y)*scale)
            actor.ConformToGround(terrain,origin,scale,0);
        safePositions[&actor]=actor.position;
    }

    void Validate(const GroundSurface& terrain) const
    {
        for(int fps:{30,60,144})for(int sign:{-1,1})for(float speed:{MovementSettings::WalkSpeed,MovementSettings::RunSpeed})
            for(int lane=0;lane<5;++lane)
            {
                CharacterController actor;actor.height=MovementSettings::CharacterHeight*scale;
                const float r=actor.height/scale*.12f;
                const float x=glm::mix(entrance.min.x+r+.1f,entrance.max.x-r-.1f,lane/4.0f);
                const float start=sign>0?entrance.min.y-1:entrance.max.y+1;
                const float end=sign>0?entrance.max.y+1:entrance.min.y-1;
                float y=origin.y;
                if(!terrain.SampleCharacter(x,start,origin.y,y,0,3))throw std::runtime_error("Entrada sin piso de partida.");
                actor.position=(glm::vec3(x,y,start)-origin)*scale;
                int frames=0;
                while(sign*(end-(actor.position.z/scale+origin.z))>.001f&&frames++<fps*10)
                {
                    const auto before=actor.position;
                    actor.position.z=sign>0?std::min((end-origin.z)*scale,before.z+speed*scale/fps):
                        std::max((end-origin.z)*scale,before.z-speed*scale/fps);
                    actor.velocity={0,0,sign*speed*scale};Resolve(actor,before,terrain);
                    actor.ConformToGround(terrain,origin,scale,1.0f/fps);
                    if(actor.position.y<(entranceFloor-origin.y-.025f)*scale)
                        throw std::runtime_error("Caida en la conexion Zocalo/Pasillo: FPS="+std::to_string(fps)+
                            ", carril="+std::to_string(lane)+", XZ="+std::to_string(actor.position.x/scale+origin.x)+
                            ","+std::to_string(actor.position.z/scale+origin.z)+", Y="+std::to_string(actor.position.y/scale+origin.y));
                }
                if(frames>=fps*10)throw std::runtime_error("El limite bloquea la entrada al Pasillo.");
                safePositions.erase(&actor);
            }
        // Every exterior AABB is tested with a long step and oblique impacts.
        for(const auto& box:wallBoxes)for(int fps:{30,60,144})
        {
            const auto size=box.max-box.min,mid=(box.min+box.max)*.5f;
            const int axis=size.x<size.z?0:2;
            for(int sign:{-1,1})
            {
                CharacterController actor;actor.height=MovementSettings::CharacterHeight*scale;
                actor.position={mid.x,0,mid.z};actor.position[axis]=sign>0?box.min[axis]-actor.height*.12f-scale:
                    box.max[axis]+actor.height*.12f+scale;
                const auto before=actor.position;actor.velocity=glm::vec3(0);actor.velocity[axis]=sign*4000.0f;
                actor.position+=actor.velocity/static_cast<float>(fps);walls.Resolve(actor,before);
                if((sign>0&&actor.position[axis]>box.min[axis]-actor.height*.12f+.01f)||
                    (sign<0&&actor.position[axis]<box.max[axis]+actor.height*.12f-.01f))
                    throw std::runtime_error("Se puede atravesar una pared exterior.");
            }
        }
        std::cout<<"Limites OK: puente en 5 carriles, caminar/correr ida y vuelta a 30/60/144 FPS; AABB sin tunneling."<<std::endl;
    }

private:
    glm::vec3 origin=glm::vec3(0);float scale=1;
    Rect plaza{},entrance{};float entranceFloor=0;std::vector<Rect> areas;
    SolidObstacles walls;std::vector<SolidObstacles::Box> wallBoxes;
    mutable std::unordered_map<const CharacterController*,glm::vec3> safePositions;

    bool Contains(glm::vec2 p) const
    {for(const auto& r:areas)if(p.x>=r.min.x&&p.x<=r.max.x&&p.y>=r.min.y&&p.y<=r.max.y)return true;return false;}

    bool Supported(const CharacterController& actor,const glm::vec3& world,float referenceWorldY,
        const GroundSurface& terrain,float& floor) const
    {
        const auto p=world/scale+origin;const float radius=actor.height/scale*.12f,d=radius*.70710678f;
        const float reference=referenceWorldY/scale+origin.y,step=std::max(actor.height/scale*.8f,MovementSettings::CharacterHeight*.8f);
        const std::array<glm::vec2,9> offsets={glm::vec2(0),{radius,0},{-radius,0},{0,radius},{0,-radius},{d,d},{-d,d},{d,-d},{-d,-d}};
        floor=-1e20f;
        for(const auto& offset:offsets)
        {
            const auto q=glm::vec2(p.x,p.z)+offset;float hit=0;
            if(!Contains(q)||!terrain.SampleCharacter(q.x,q.y,reference,hit,0,step)||hit<reference-step-.05f)return false;
            floor=std::max(floor,hit);
        }
        return true;
    }

    glm::vec3 SweepSupport(const CharacterController& actor,const glm::vec3& start,const glm::vec3& target,const GroundSurface& terrain) const
    {
        const float distance=glm::length(glm::vec2(target.x-start.x,target.z-start.z));
        const int samples=std::max(1,static_cast<int>(std::ceil(distance/(.15f*scale))));
        float last=0,floor=0;
        for(int i=1;i<=samples;++i)
        {
            const float t=static_cast<float>(i)/samples;
            if(!Supported(actor,glm::mix(start,target,t),start.y,terrain,floor))
            {
                float lo=last,hi=t;
                for(int j=0;j<12;++j){const float mid=(lo+hi)*.5f;
                    if(Supported(actor,glm::mix(start,target,mid),start.y,terrain,floor))lo=mid;else hi=mid;}
                return glm::mix(start,target,lo);
            }
            last=t;
        }
        return target;
    }

    void FinishWalls()
    {
        // Small export seams are joined; exact ground tests still reject real voids.
        for(auto& a:areas){a.min-=glm::vec2(.015f);a.max+=glm::vec2(.015f);}
        std::sort(areas.begin(),areas.end(),[](const Rect& a,const Rect& b){return(a.max.x-a.min.x)*(a.max.y-a.min.y)>(b.max.x-b.min.x)*(b.max.y-b.min.y);});
        std::vector<Rect> reduced;
        for(const auto& a:areas)
        {
            bool covered=false;
            for(const auto& b:reduced)if(a.min.x>=b.min.x&&a.max.x<=b.max.x&&a.min.y>=b.min.y&&a.max.y<=b.max.y){covered=true;break;}
            if(!covered)reduced.push_back(a);
        }
        areas=std::move(reduced);wallBoxes.clear();
        for(size_t i=0;i<areas.size();++i)for(int axis=0;axis<2;++axis)for(int sign:{-1,1})
        {
            const auto& a=areas[i];const int along=1-axis;
            const float edge=sign<0?a.min[axis]:a.max[axis];
            std::vector<glm::vec2> spans{{a.min[along],a.max[along]}};
            for(size_t j=0;j<areas.size();++j)if(j!=i)
            {
                const auto& b=areas[j];
                if(sign<0?!(b.min[axis]<edge&&b.max[axis]>=edge):!(b.max[axis]>edge&&b.min[axis]<=edge))continue;
                std::vector<glm::vec2> rest;
                for(const auto& s:spans)
                {
                    if(b.max[along]<=s.x||b.min[along]>=s.y){rest.push_back(s);continue;}
                    if(b.min[along]>s.x)rest.push_back({s.x,b.min[along]});
                    if(b.max[along]<s.y)rest.push_back({b.max[along],s.y});
                }
                spans=std::move(rest);
            }
            for(const auto& s:spans)if(s.y-s.x>.001f)
            {
                glm::vec2 lo,hi;lo[along]=s.x;hi[along]=s.y;
                lo[axis]=sign<0?edge-.4f:edge;hi[axis]=sign<0?edge:edge+.4f;
                wallBoxes.push_back({{(lo.x-origin.x)*scale,-1000000,(lo.y-origin.z)*scale},
                    {(hi.x-origin.x)*scale,1000000,(hi.y-origin.z)*scale}});
            }
        }
        walls.SetWalls(wallBoxes,scale);
    }
};
