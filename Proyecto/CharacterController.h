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
        hadInput = false;
        fallVelocity = 0.0f;
        if (animator) animator->Reset();
    }
    void Attach(SkeletalAnimator* rig) { animator = rig; if (!rig) throw std::runtime_error("Personaje sin esqueleto."); rig->Reset(); }
    void SetFrozen(bool value) { frozen = value; }
    bool IsFrozen() const { return frozen; }
    void Update(glm::vec2 input, glm::vec3 cameraForward, bool sprint, float dt, float worldScale)
    {
        if (frozen) return;
        const bool moving = glm::dot(input, input) > 0.001f;
        if (moving && !hadInput)
        {
            cameraForward.y = 0.0f;
            movementForward = glm::dot(cameraForward, cameraForward) > 0.001f
                ? glm::normalize(cameraForward) : glm::vec3(0, 0, 1);
        }
        hadInput = moving;
        glm::vec3 desired(0.0f);
        if (moving)
        {
            // Mantener la base durante cada pulsacion evita realimentar el giro
            // de la camara de seguimiento al mantener A/D o S.
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
            const float targetYaw = std::atan2(velocity.x, velocity.z);
            const float delta = std::remainder(targetYaw - yaw, 6.28318530718f);
            yaw += delta * (1.0f - std::exp(-12.0f * dt));
            // Proporcion respecto a la altura: FBX puede usar centimetros.
            const float cycleDistance = height * glm::mix(MovementSettings::WalkStrideHeightRatio,MovementSettings::RunStrideHeightRatio,running);
            phase = std::fmod(phase + glm::length(displacement) * 6.28318530718f / cycleDistance, 6.28318530718f);
        }
        const float weight = glm::clamp(actualSpeed / (worldScale * MovementSettings::WalkSpeed), 0.0f, 1.0f);
        gait = glm::mix(gait, weight, 1.0f - std::exp(-9.0f * dt));
        if (animator) animator->Update(dt, phase, gait, running);
    }
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
    bool hadInput = false;
    glm::vec3 movementForward = glm::vec3(0, 0, 1);
};

class FollowCamera
{
public:
    float pitch = 18.0f, orbit = 18.0f, distance = MovementSettings::FollowDistance;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 target = glm::vec3(0.0f);
    void Mouse(float x, float y)
    {
        orbit = glm::clamp(orbit - x * 0.15f, -65.0f, 65.0f);
        pitch = glm::clamp(pitch + y * 0.15f, 0.0f, 60.0f);
    }
    void Scroll(float amount) { distance = glm::clamp(distance - amount * 0.4f, 3.8f, 12.0f); }
    void Reset() { pitch = 18.0f; orbit = 18.0f; distance = MovementSettings::FollowDistance; initialized = false; }
    void Update(const CharacterController& player, float dt, float scale, bool snap = false)
    {
        const glm::vec3 desiredTarget = player.position + glm::vec3(0, player.height * 0.68f, 0);
        const float angle = player.yaw + glm::radians(orbit);
        const float elevation = glm::radians(pitch);
        const glm::vec3 offset(-std::sin(angle) * std::cos(elevation),
            std::sin(elevation), -std::cos(angle) * std::cos(elevation));
        const glm::vec3 desiredPosition = desiredTarget + offset * distance * scale;
        const float blend = !initialized || snap ? 1.0f : 1.0f - std::exp(-9.0f * dt);
        target = glm::mix(target, desiredTarget, blend);
        position = glm::mix(position, desiredPosition, blend);
        initialized = true;
    }
    glm::mat4 View() const { return glm::lookAt(position, target, glm::vec3(0, 1, 0)); }
    glm::vec3 Forward() const { return glm::normalize(target - position); }
private:
    bool initialized = false;
};
