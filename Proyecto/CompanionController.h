#pragma once
#include <algorithm>
#include <cmath>
#include "CharacterController.h"

class CompanionController:public CharacterController
{
public:
    enum class State{Following,Special};
    void AttachCompanion(SkeletalAnimator* value)
    {
        rig=value;Attach(value);
        if(!rig->HasClip("Special"))throw std::runtime_error("El xolo necesita el clip Special.");
    }
    glm::vec3 FollowTarget(const CharacterController& owner,float scale) const
    {
        return owner.position+glm::cross(owner.Forward(),glm::vec3(0,1,0))*(1.8f*scale)-owner.Forward()*(.8f*scale);
    }
    void ResetBeside(const CharacterController& owner,float scale)
    {
        SetFrozen(false);Reset();state=State::Following;clock=0;
        position=FollowTarget(owner,scale);yaw=owner.yaw;
    }
    bool TriggerSpecial()
    {
        if(!rig||state==State::Special)return false;
        state=State::Special;clock=0;actionStart=position;actionForward=Forward();return true;
    }
    bool IsSpecial() const{return state==State::Special;}
    float ActionTime() const{return clock;}
    void UpdateCompanion(const CharacterController& owner,const GroundSurface* ground,const glm::vec3& origin,float scale,float dt)
    {
        if(!rig)return;
        // Subpasos mantienen estable el seguimiento y evitan saltarse bordes.
        const int steps=std::max(1,static_cast<int>(std::ceil(dt*120)));
        for(int step=0;step<steps;++step)Tick(owner,ground,origin,scale,dt/steps);
    }
private:
    SkeletalAnimator* rig=nullptr;State state=State::Following;
    float clock=0;glm::vec3 actionStart=glm::vec3(0),actionForward=glm::vec3(0,0,1);
    static float Ease(float t){t=glm::clamp(t,0.0f,1.0f);return t*t*t*(t*(t*6-15)+10);}
    void Tick(const CharacterController& owner,const GroundSurface* ground,const glm::vec3& origin,float scale,float dt)
    {
        const glm::vec3 before=position;
        if(state==State::Following)
        {
            glm::vec3 error=FollowTarget(owner,scale)-position;error.y=0;
            glm::vec3 desired=owner.IsFrozen()?glm::vec3(0):owner.velocity;
            desired.y=0;desired+=error*3.5f;
            if(glm::length(error)<.10f*scale&&glm::length(owner.velocity)<.05f*scale)desired=glm::vec3(0);
            const float maxSpeed=9.0f*scale,speed=glm::length(desired);
            if(speed>maxSpeed)desired*=maxSpeed/speed;
            const float decay=std::exp(-9.0f*dt);
            position+=desired*dt+(velocity-desired)*((1-decay)/9.0f);
            velocity=desired+(velocity-desired)*decay;
            const float targetYaw=glm::length(velocity)>.08f*scale?std::atan2(velocity.x,velocity.z):owner.yaw;
            yaw+=std::remainder(targetYaw-yaw,6.28318530718f)*(1-std::exp(-10.0f*dt));
        }
        else
        {
            clock=std::min(clock+dt,rig->Duration("Special"));
            const float t=clock*3.8f/rig->Duration("Special");
            // Keyframe de traslacion: un paso de carrera despues del giro.
            const glm::vec3 dash=actionForward*(1.6f*scale*Ease((t-2.70f)/.90f));
            position.x=actionStart.x+dash.x;position.z=actionStart.z+dash.z;
            velocity=(position-before)/std::max(.00001f,dt);velocity.y=0;
        }
        glm::vec3 delta=position-before;delta.y=0;
        const float speed=glm::length(delta)/std::max(.00001f,dt)/scale;
        running=glm::mix(running,glm::clamp((speed-2.0f)/3.5f,0.0f,1.0f),1-std::exp(-7.0f*dt));
        gait=glm::mix(gait,state==State::Special?0.0f:glm::clamp(speed/1.2f,0.0f,1.0f),1-std::exp(-10.0f*dt));
        phase=std::fmod(phase+glm::length(delta)*6.28318530718f/(height*glm::mix(.85f,1.25f,running)),6.28318530718f);
        if(ground)ConformToGround(*ground,origin,scale,dt,MovementSettings::CharacterHeight*.8f);
        if(state==State::Special)
        {
            const float duration=rig->Duration("Special");
            const float blend=Ease(clock/.18f)*Ease((duration-clock)/.22f);
            rig->UpdateLayered(dt,phase,gait,running,"Special",clock,blend);
            if(clock>=duration){state=State::Following;velocity=glm::vec3(0);}
        }
        else rig->Update(dt,phase,gait,running);
    }
};
