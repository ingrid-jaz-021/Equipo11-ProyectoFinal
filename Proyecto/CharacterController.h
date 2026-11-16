#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SkeletalAnimator.h"
#include "GroundSurface.h"
#include "MovementSettings.h"

class CharacterController
{
public:
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    float yaw = 0.0f; // Frente del OBJ: +Z.
    float phase = 0.0f;
    float gait = 0.0f;
    float running = 0.0f;
    float height = 28.0f;
    float modelScale = 10.0f;
    float modelFloor = 0.004732f;

    void Configure(const glm::vec3& min, const glm::vec3& max, float targetHeight)
    {
        height = targetHeight;
        modelFloor = min.y;
        modelScale = height / std::max(0.001f, max.y - min.y);
    }
    void Reset()
    {
        position = glm::vec3(0.0f);
        velocity = glm::vec3(0.0f);
        yaw = phase = gait = running = 0.0f;
        fallVelocity = 0.0f;
        if (animator) animator->Reset();
    }
    void Attach(SkeletalAnimator* rig) { animator = rig; if (!rig) throw std::runtime_error("Personaje sin esqueleto."); rig->Reset(); }
    void SetFrozen(bool value) { frozen = value; }
    bool IsFrozen() const { return frozen; }
    void PreparePassenger(){Reset();SetFrozen(true);} // Pose Idle fija; sin reloj de animacion a bordo.
    void Update(glm::vec2 input, glm::vec3 cameraForward, bool sprint, float dt, float worldScale,bool animate=true)
    {
        if (frozen) return;
        const bool moving = glm::dot(input, input) > 0.001f;
        cameraForward.y = 0.0f;
        const glm::vec3 movementForward = glm::dot(cameraForward,cameraForward)>.001f
            ?glm::normalize(cameraForward):Forward();
        // The entire model follows the independent camera heading, including
        // while idle, reversing or strafing. Keyboard never changes camera yaw.
        const float targetYaw=std::atan2(movementForward.x,movementForward.z);
        yaw=std::remainder(yaw+std::remainder(targetYaw-yaw,6.28318530718f)*
            (1-std::exp(-12.0f*dt)),6.28318530718f);
        glm::vec3 desired(0.0f);
        if (moving)
        {
            // Recompute from camera yaw every step, even with a key held.
            input = glm::normalize(input);
            const glm::vec3 right = glm::cross(movementForward, glm::vec3(0, 1, 0));
            desired = movementForward * input.y + right * input.x;
        }
        running = glm::mix(running, sprint && moving ? 1.0f : 0.0f, 1.0f - std::exp(-4.5f * dt));
        const float speed = worldScale * glm::mix(MovementSettings::WalkSpeed, MovementSettings::RunSpeed, running);
        const glm::vec3 target = desired * speed;
        const float damping = moving ? 10.0f : 14.0f;
        const float decay = std::exp(-damping * dt);
        // Integral analitica: desplazamiento estable a 30, 60 o 144 FPS.
        const glm::vec3 displacement = target * dt + (velocity - target) * ((1.0f - decay) / damping);
        position += displacement;
        velocity = target + (velocity - target) * decay;
        const float actualSpeed = glm::length(velocity);
        if (actualSpeed > worldScale * 0.02f)
        {
            // Proporcion respecto a la altura: FBX puede usar centimetros.
            const float cycleDistance = height * glm::mix(MovementSettings::WalkStrideHeightRatio,MovementSettings::RunStrideHeightRatio,running);
            const float advance=glm::length(displacement)*6.28318530718f/cycleDistance;
            const float cadence=glm::mix(1.7f,2.8f,running);
            phase=std::fmod(phase+std::min(advance,dt*6.28318530718f*cadence),6.28318530718f);
        }
        const float weight = glm::clamp(actualSpeed / (worldScale * MovementSettings::WalkSpeed), 0.0f, 1.0f);
        gait = glm::mix(gait, weight, 1.0f - std::exp(-9.0f * dt));
        if(!moving&&weight<.0001f&&gait<.002f){velocity=glm::vec3(0);gait=running=0;}
        if(animate)AnimateFrame(dt);
    }
    void AnimateFrame(float dt){if(animator&&!frozen)animator->Update(dt,phase,gait,running);}
    glm::vec3 Forward() const { return glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw)); }
    void ConformToGround(const GroundSurface& ground,const glm::vec3& origin,float scale,float dt,float minStepHeight=0)
    {
        if(frozen) return;
        const glm::vec3 source=position/scale+origin;
        float floor=0;
        const float radius=height/scale*0.12f;
        const float stepHeight=std::max(height/scale*.80f,minStepHeight);
        const bool supported=ground.SampleCharacter(source.x,source.z,source.y,floor,radius,stepHeight);
        const float target=(floor-origin.y)*scale;
        if(supported&&target>=position.y-0.001f)
        {
            // Subir de inmediato evita atravesar el bloque incluso un frame.
            position.y=target;fallVelocity=0.0f;
        }
        else
        {
            const float gravity=18.0f*scale;
            position.y+=fallVelocity*dt-0.5f*gravity*dt*dt;
            fallVelocity-=gravity*dt;
            if(supported&&position.y<=target){position.y=target;fallVelocity=0.0f;}
        }
    }
    const std::vector<glm::mat4>& Pose() const {
        static const std::vector<glm::mat4> identity(1, glm::mat4(1));
        return animator ? animator->Palette() : identity;
    }
    glm::mat4 ModelMatrix() const
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(modelScale));
        return glm::translate(model, glm::vec3(0.0f, -modelFloor, 0.0f));
    }
private:
    SkeletalAnimator* animator = nullptr;
    bool frozen = false;
    float fallVelocity = 0.0f;
};

class FollowCamera
{
public:
    float pitch = 18.0f, yaw = 0.0f, distance = MovementSettings::FollowDistance;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 target = glm::vec3(0.0f);
    void Mouse(float x, float y)
    {
        yaw=std::remainder(yaw-glm::radians(x*.15f),6.28318530718f);
        pitch=glm::clamp(pitch-y*.15f,-10.0f,72.0f);
    }
    void Scroll(float amount) { distance = glm::clamp(distance - amount * 0.4f, 3.8f, 12.0f); }
    void Reset() { pitch = 18.0f; yaw = 0.0f; distance = MovementSettings::FollowDistance; initialized = false; }
    void Update(const CharacterController& player, float dt, float scale, bool snap = false)
    {
        UpdateAnchor(player.position,player.height,dt,scale,0,snap);
    }
    void UpdateAnchor(const glm::vec3& anchor,float height,float dt,float scale,float minimumDistance,bool snap=false)
    {
        const glm::vec3 desiredTarget = anchor + glm::vec3(0, height * 0.68f, 0);
        const float angle = yaw;
        const float elevation = glm::radians(pitch);
        const glm::vec3 offset(-std::sin(angle) * std::cos(elevation),
            std::sin(elevation), -std::cos(angle) * std::cos(elevation));
        const float blend = !initialized || snap ? 1.0f : 1.0f - std::exp(-9.0f * dt);
        target = glm::mix(target, desiredTarget, blend);
        // Smooth follow translation; mouse orientation remains responsive and
        // independent of body/boat rotation, avoiding a feedback loop.
        position=target+offset*std::max(distance,minimumDistance)*scale;
        position.y=std::max(position.y,anchor.y+height*.18f);
        initialized = true;
    }
    glm::mat4 View() const { return glm::lookAt(position, target, glm::vec3(0, 1, 0)); }
    glm::vec3 Forward() const { return {std::sin(yaw),0,std::cos(yaw)}; }
private:
    bool initialized = false;
};
