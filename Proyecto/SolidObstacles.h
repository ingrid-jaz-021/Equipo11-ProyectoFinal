#pragma once
#include <array>
#include <cmath>
#include <vector>
#include "Model.h"
#include "CharacterController.h"

// Barrido continuo de la huella del personaje contra cada AABB: no hay tunneling.
class SolidObstacles
{
public:
    struct Box{glm::vec3 min,max;};
    void Build(const Model& scene,const glm::vec3& origin,float scale)
    {
        boxes.clear();epsilon=.002f*scale;
        for(const auto& b:scene.DoorBoxes())boxes.push_back({(b.min-origin)*scale,(b.max-origin)*scale});
        if(boxes.size()!=8)throw std::runtime_error("Se necesitan ocho pilares solidos.");
        openings.clear();for(const auto& o:scene.DoorOpenings())openings.push_back({(o.min-origin)*scale,(o.max-origin)*scale,o.normalAxis});
    }
    void Resolve(CharacterController& actor,const glm::vec3& before) const
    {
        if(actor.IsFrozen())return;
        const float radius=actor.height*.12f;
        glm::vec2 p(before.x,before.z),remaining(actor.position.x-before.x,actor.position.z-before.z);
        const float feet=std::min(before.y,actor.position.y),head=std::max(before.y,actor.position.y)+actor.height;
        for(int iteration=0;iteration<8;++iteration)
        {
            // Recuperacion determinista si una reubicacion/reinicio deja la huella dentro.
            for(const auto& b:boxes)
            {
                if(head<b.min.y||feet>b.max.y)continue;
                const auto lo=glm::vec2(b.min.x,b.min.z)-radius,hi=glm::vec2(b.max.x,b.max.z)+radius;
                if(p.x>lo.x&&p.x<hi.x&&p.y>lo.y&&p.y<hi.y)
                {
                    const std::array<float,4> gaps={p.x-lo.x,hi.x-p.x,p.y-lo.y,hi.y-p.y};
                    const int side=static_cast<int>(std::min_element(gaps.begin(),gaps.end())-gaps.begin());
                    const int axis=side/2;p[axis]=(side%2?hi[axis]+epsilon:lo[axis]-epsilon);remaining[axis]=0;actor.velocity[axis==0?0:2]=0;
                }
            }
            if(glm::dot(remaining,remaining)<1e-10f)break;
            float first=1;glm::vec2 normal(0);bool hit=false;
            for(const auto& b:boxes)
            {
                if(head<b.min.y||feet>b.max.y)continue;
                const auto lo=glm::vec2(b.min.x,b.min.z)-radius,hi=glm::vec2(b.max.x,b.max.z)+radius;
                float enter=-1e20f,exit=1e20f;glm::vec2 n(0);bool miss=false;
                for(int axis=0;axis<2;++axis)
                {
                    if(std::abs(remaining[axis])<1e-8f){if(p[axis]<lo[axis]||p[axis]>hi[axis])miss=true;continue;}
                    float a=(lo[axis]-p[axis])/remaining[axis],c=(hi[axis]-p[axis])/remaining[axis];
                    if(a>c)std::swap(a,c);
                    if(a>enter){enter=a;n=glm::vec2(0);n[axis]=remaining[axis]>0?-1.0f:1.0f;}
                    exit=std::min(exit,c);
                }
                if(!miss&&enter<=exit&&exit>=0&&enter>=0&&enter<=first){first=enter;normal=n;hit=true;}
            }
            if(!hit){p+=remaining;remaining=glm::vec2(0);break;}
            p+=remaining*first+normal*epsilon;remaining*=1-first;
            const float into=glm::dot(remaining,normal);if(into<0)remaining-=normal*into;
            const glm::vec3 worldNormal(normal.x,0,normal.y);const float speed=glm::dot(actor.velocity,worldNormal);
            if(speed<0)actor.velocity-=worldNormal*speed;
        }
        actor.position.x=p.x;actor.position.z=p.y;
    }
    void Validate(const GroundSurface& terrain,const glm::vec3& origin,float scale) const
    {
        if(boxes.size()!=8||openings.size()!=4)throw std::runtime_error("Cantidad de pilares/pasos incorrecta.");
        for(const auto& box:boxes)for(int axis=0;axis<2;++axis)for(int sign:{-1,1})for(int fps:{30,60,144})
        {
            CharacterController test;test.height=28;const float r=test.height*.12f;
            const int component=axis==0?0:2;const auto center=(box.min+box.max)*.5f;
            test.position={center.x,box.min.y,center.z};test.position[component]=(sign>0?box.min[component]-r-20:box.max[component]+r+20);
            for(int f=0;f<fps;++f)
            {
                auto before=test.position;test.velocity=glm::vec3(0);test.velocity[component]=sign*4000.0f;
                test.position+=test.velocity/static_cast<float>(fps);Resolve(test,before);
                if((sign>0&&test.position[component]>box.min[component]-r+.01f)||(sign<0&&test.position[component]<box.max[component]+r-.01f))
                    throw std::runtime_error("El barrido permite atravesar una puerta.");
            }
        }
        for(const auto& opening:openings)for(int sign:{-1,1})for(int fps:{30,60,144})for(float speed:{MovementSettings::WalkSpeed,MovementSettings::RunSpeed})
        {
            CharacterController test;test.height=MovementSettings::CharacterHeight*scale;
            const int axis=opening.axis,span=axis==0?2:0;
            if(opening.max[span]-opening.min[span]<test.height*.24f+scale)throw std::runtime_error("Hueco de arco demasiado estrecho.");
            test.position=(opening.min+opening.max)*.5f;test.position.y=opening.min.y;
            const float start=sign>0?opening.min[axis]-scale:opening.max[axis]+scale;
            const float end=sign>0?opening.max[axis]+scale:opening.min[axis]-scale;
            test.position[axis]=start;
            while(sign*(end-test.position[axis])>.001f)
            {
                const auto before=test.position;const float next=sign>0?std::min(end,before[axis]+speed*scale/fps):std::max(end,before[axis]-speed*scale/fps);
                test.position[axis]=next;test.velocity=glm::vec3(0);test.velocity[axis]=sign*speed*scale;
                Resolve(test,before);
                if(std::abs(test.position[axis]-next)>.001f)throw std::runtime_error("Pared invisible en el centro de un arco.");
                test.ConformToGround(terrain,origin,scale,1.0f/fps);
                if(std::abs(test.position.y-opening.min.y)>.02f)throw std::runtime_error("Escalon inesperado bajo el arco.");
            }
        }
        std::cout<<"Arcos OK: 8 AABB solo en pilares; 4 centros transitables caminando/corriendo en ambos sentidos a 30/60/144 FPS; sin tunneling a 4000 unidades/s."<<std::endl;
    }
private:
    struct Opening{glm::vec3 min,max;int axis;};
    std::vector<Box> boxes;std::vector<Opening> openings;float epsilon=.02f;
};
