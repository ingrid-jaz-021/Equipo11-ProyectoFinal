#pragma once
#include <array>
#include <cmath>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct SkinVertex
{
    glm::ivec4 joints = glm::ivec4(0);
    glm::vec4 weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
};

class ProceduralRig
{
public:
    enum Bone { Root, ThighL, ShinL, FootL, ThighR, ShinR, FootR, ArmL, ElbowL, ArmR, ElbowR, Count };
    using Pose = std::array<glm::mat4, Count>;

    static SkinVertex Weights(const glm::vec3& p, const std::string& material)
    {
        SkinVertex result;
        const bool left = p.x >= 0.0f;
        if (material == "skin.002" || (material == "Tshirt" && p.y > 0.62f && std::abs(p.x) > 0.14f))
        {
            const float arm = material == "skin.002" ? 1.0f : glm::smoothstep(0.14f, 0.21f, std::abs(p.x));
            const float elbow = glm::smoothstep(0.33f, 0.40f, std::abs(p.x));
            result.joints = glm::ivec4(Root, left ? ArmL : ArmR, left ? ElbowL : ElbowR, Root);
            result.weights = glm::vec4(1.0f - arm, arm * (1.0f - elbow), arm * elbow, 0.0f);
        }
        else if (p.y < 0.47f && material != "Tshirt")
        {
            const float leg = 1.0f - glm::smoothstep(0.38f, 0.46f, p.y);
            const float knee = 1.0f - glm::smoothstep(0.22f, 0.29f, p.y);
            const float foot = 1.0f - glm::smoothstep(0.085f, 0.13f, p.y);
            result.joints = glm::ivec4(Root, left ? ThighL : ThighR, left ? ShinL : ShinR, left ? FootL : FootR);
            result.weights = glm::vec4(1.0f - leg, leg * (1.0f - knee), leg * knee * (1.0f - foot), leg * knee * foot);
        }
        return result;
    }

    struct Motion
    {
        Pose bones;
        float rootY = 0.0f;
        float roll = 0.0f;
        float lean = 0.0f;
        float twist = 0.0f;
    };

    static float CycleDistance(float run)
    {
        // Distancia de un ciclo completo en el espacio local de esta anatomia.
        return glm::mix(2.0f * 0.17f / 0.60f, 2.0f * 0.21f / 0.36f, glm::clamp(run, 0.0f, 1.0f));
    }

    static Motion Sample(float phase, float weight, float run)
    {
        Motion result;
        for (auto& bone : result.bones) bone = glm::mat4(1.0f);
        run = glm::clamp(run, 0.0f, 1.0f);
        const float blend = Smooth(glm::clamp(weight, 0.0f, 1.0f));
        const float sine = std::sin(phase), cosine = std::cos(phase);
        const float doubleCosine = 2.0f * cosine * cosine - 1.0f;
        const float duty = glm::mix(0.60f, 0.36f, run);
        const float reach = 0.5f * CycleDistance(run) * duty;
        const float lift = glm::mix(0.045f, 0.09f, run);
        result.rootY = -(glm::mix(0.006f, 0.012f, run) +
            glm::mix(0.040f, 0.060f, run) * (0.5f + 0.5f * doubleCosine)) * blend;
        result.roll = sine * glm::mix(0.012f, 0.022f, run) * blend;
        result.lean = glm::mix(0.018f, 0.065f, run) * blend;
        result.twist = sine * glm::mix(0.018f, 0.035f, run) * blend;

        for (int side = 0; side < 2; ++side)
        {
            const bool left = side == 0;
            const float sign = left ? 1.0f : -1.0f;
            float cycle = phase / 6.28318530718f + (left ? 0.0f : 0.5f);
            cycle -= std::floor(cycle);
            float footZ, footLift = 0.0f, footPitch;
            if (cycle < duty)
            {
                const float t = cycle / duty;
                // El pie retrocede a velocidad constante respecto a la cadera
                // durante el apoyo: evita el deslizamiento del ciclo sinusoidal.
                footZ = glm::mix(reach, -reach, t);
                footPitch = glm::radians(9.0f) * (1.0f - Smooth(glm::clamp(t / 0.18f, 0.0f, 1.0f))) -
                    glm::radians(17.0f) * Smooth(glm::clamp((t - 0.78f) / 0.22f, 0.0f, 1.0f));
            }
            else
            {
                const float t = (cycle - duty) / (1.0f - duty);
                // Hermite quintico: misma velocidad y aceleracion al despegar
                // y al aterrizar. Sin max(cos,0) ni quiebres en las rodillas.
                const float tangent = -2.0f * reach * (1.0f - duty) / duty;
                footZ = -reach + tangent * t + (2.0f * reach - tangent) * Smooth(t);
                const float bump = t * (1.0f - t);
                footLift = 64.0f * bump * bump * bump * lift;
                footPitch = t < 0.5f
                    ? glm::mix(glm::radians(-17.0f), glm::radians(-4.0f), Smooth(t * 2.0f))
                    : glm::mix(glm::radians(-4.0f), glm::radians(9.0f), Smooth(t * 2.0f - 1.0f));
            }

            const glm::vec3 hip(sign * 0.095f, 0.43f, 0.015f);
            const glm::vec3 kneePivot(sign * 0.105f, 0.255f, 0.015f);
            const glm::vec3 ankle(sign * 0.105f, 0.105f, 0.025f);
            constexpr float thighLength = 0.175f, shinLength = 0.150f;
            const float dz = footZ + ankle.z - hip.z;
            const float dy = ankle.y + footLift - result.rootY - hip.y;
            const float length = glm::clamp(std::sqrt(dz * dz + dy * dy), 0.09f,
                thighLength + shinLength - 0.001f);
            const float knee = glm::clamp(std::acos(glm::clamp(
                (length * length - thighLength * thighLength - shinLength * shinLength) /
                (2.0f * thighLength * shinLength), -1.0f, 1.0f)), 0.0f, glm::mix(1.65f, 1.85f, run));
            const float thigh = glm::clamp(std::atan2(-dz, -dy) -
                std::atan2(shinLength * std::sin(knee), thighLength + shinLength * std::cos(knee)),
                -0.85f, 0.72f);
            const float ankleAngle = glm::clamp(footPitch - thigh - knee, -0.65f, 0.65f);
            const int thighId = left ? ThighL : ThighR, shinId = left ? ShinL : ShinR, footId = left ? FootL : FootR;
            result.bones[thighId] = Around(hip, Rotate(thigh * blend, glm::vec3(1, 0, 0)));
            result.bones[shinId] = result.bones[thighId] * Around(kneePivot, Rotate(knee * blend, glm::vec3(1, 0, 0)));
            result.bones[footId] = result.bones[shinId] * Around(ankle, Rotate(ankleAngle * blend, glm::vec3(1, 0, 0)));

            const glm::vec3 shoulder(sign * 0.17f, 0.685f, 0.025f);
            const glm::vec3 elbow(sign * 0.37f, 0.685f, 0.025f);
            const int upper = left ? ArmL : ArmR, lower = left ? ElbowL : ElbowR;
            const float armSwing = sign * (sine * 0.94f + cosine * 0.20f) * glm::mix(0.16f, 0.30f, run) * blend;
            const float elbowFlex = glm::mix(0.18f, 0.68f, run * blend) +
                sign * cosine * 0.06f * blend;
            result.bones[upper] = Around(shoulder, Rotate(armSwing, glm::vec3(1, 0, 0)) *
                Rotate(-sign * glm::radians(78.0f - 3.0f * run * blend), glm::vec3(0, 0, 1)));
            result.bones[lower] = result.bones[upper] * Around(elbow,
                Rotate(-sign * elbowFlex, glm::vec3(0, 1, 0)));
        }
        return result;
    }

    static Pose Animate(float phase, float weight, float run) { return Sample(phase, weight, run).bones; }
private:
    static float Smooth(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static glm::mat4 Rotate(float radians, const glm::vec3& axis)
    {
        return glm::rotate(glm::mat4(1.0f), radians, axis);
    }
    static glm::mat4 Around(const glm::vec3& pivot, const glm::mat4& rotation)
    {
        return glm::translate(glm::mat4(1.0f), pivot) * rotation *
            glm::translate(glm::mat4(1.0f), -pivot);
    }
};
