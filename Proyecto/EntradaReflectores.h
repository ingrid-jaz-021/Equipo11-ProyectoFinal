#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include "Model.h"
#include "Shader.h"
#include "GroundSurface.h"

class EntradaReflectores
{
public:
    static constexpr float ReflectorModelScale = 0.55f;

    struct Reflector
    {
        glm::vec3 sourcePos;
        glm::vec3 worldPos;
        float yaw;
        float pitch;
        glm::mat4 modelMatrix;
        glm::vec3 spotPos;
        glm::vec3 spotDir;
        glm::vec3 spotColor;
        float range;
        float innerCutoff;
        float outerCutoff;
    };

    std::vector<Reflector> instances;

    bool Configured() const { return instances.size() == 2; }

    void Build(const GroundSurface* ground, const glm::vec3& sourceOrigin, float worldScale)
    {
        instances.clear();

        // 1. Reflector Izquierdo: plano en el suelo, retrasado hacia atras (en +Z)
        // Base 100% horizontal apoyada en el piso (pitch = 0), orientado con yaw hacia el letrero azul y ajolote
        glm::vec3 srcLeft(-128.0f, 1.7764f, 257.5f);
        float floorLeft = srcLeft.y;
        if (ground && ground->Sample(srcLeft.x, srcLeft.z, 2.0f, floorLeft))
            srcLeft.y = floorLeft;

        glm::vec3 worldLeft = (srcLeft - sourceOrigin) * worldScale;
        glm::vec3 targetLeftSrc(-122.0f, 3.8f, 247.91f);
        glm::vec3 worldTargetLeft = (targetLeftSrc - sourceOrigin) * worldScale;

        glm::vec3 deltaLeft = worldTargetLeft - (worldLeft + glm::vec3(0.0f, 10.0f, 0.0f));
        glm::vec3 dirLeft = glm::normalize(deltaLeft);

        float dxLeft = targetLeftSrc.x - srcLeft.x;
        float dzLeft = targetLeftSrc.z - srcLeft.z;
        float yawLeft = glm::degrees(std::atan2(dxLeft, dzLeft));

        float mScale = ReflectorModelScale * worldScale;
        glm::mat4 matLeft = glm::translate(glm::mat4(1.0f), worldLeft);
        matLeft = glm::rotate(matLeft, glm::radians(yawLeft), glm::vec3(0.0f, 1.0f, 0.0f));
        // Base perfectamente plana en el suelo: sin inclinacion (pitch = 0)
        matLeft = glm::scale(matLeft, glm::vec3(mScale));

        Reflector leftRefl;
        leftRefl.sourcePos = srcLeft;
        leftRefl.worldPos = worldLeft;
        leftRefl.yaw = yawLeft;
        leftRefl.pitch = 0.0f;
        leftRefl.modelMatrix = matLeft;
        leftRefl.spotPos = worldLeft + glm::vec3(0.0f, 10.0f, 0.0f) + dirLeft * 3.0f;
        leftRefl.spotDir = dirLeft;
        leftRefl.spotColor = glm::vec3(4.0f, 3.6f, 2.8f); // Luz calida y potente
        leftRefl.range = 180.0f;
        leftRefl.innerCutoff = std::cos(glm::radians(24.0f));
        leftRefl.outerCutoff = std::cos(glm::radians(48.0f));
        instances.push_back(leftRefl);

        // 2. Reflector Derecho: plano en el suelo, retrasado hacia atras (en +Z)
        // Base 100% horizontal apoyada en el piso (pitch = 0), orientado con yaw cruzado hacia el ajolote y entrada
        glm::vec3 srcRight(-101.5f, 1.7764f, 257.5f);
        float floorRight = srcRight.y;
        if (ground && ground->Sample(srcRight.x, srcRight.z, 2.0f, floorRight))
            srcRight.y = floorRight;

        glm::vec3 worldRight = (srcRight - sourceOrigin) * worldScale;
        glm::vec3 targetRightSrc(-118.0f, 3.2f, 247.91f);
        glm::vec3 worldTargetRight = (targetRightSrc - sourceOrigin) * worldScale;

        glm::vec3 deltaRight = worldTargetRight - (worldRight + glm::vec3(0.0f, 10.0f, 0.0f));
        glm::vec3 dirRight = glm::normalize(deltaRight);

        float dxRight = targetRightSrc.x - srcRight.x;
        float dzRight = targetRightSrc.z - srcRight.z;
        float yawRight = glm::degrees(std::atan2(dxRight, dzRight));

        glm::mat4 matRight = glm::translate(glm::mat4(1.0f), worldRight);
        matRight = glm::rotate(matRight, glm::radians(yawRight), glm::vec3(0.0f, 1.0f, 0.0f));
        // Base perfectamente plana en el suelo: sin inclinacion (pitch = 0)
        matRight = glm::scale(matRight, glm::vec3(mScale));

        Reflector rightRefl;
        rightRefl.sourcePos = srcRight;
        rightRefl.worldPos = worldRight;
        rightRefl.yaw = yawRight;
        rightRefl.pitch = 0.0f;
        rightRefl.modelMatrix = matRight;
        rightRefl.spotPos = worldRight + glm::vec3(0.0f, 10.0f, 0.0f) + dirRight * 3.0f;
        rightRefl.spotDir = dirRight;
        rightRefl.spotColor = glm::vec3(3.8f, 3.4f, 2.6f);
        rightRefl.range = 250.0f;
        rightRefl.innerCutoff = std::cos(glm::radians(24.0f));
        rightRefl.outerCutoff = std::cos(glm::radians(48.0f));
        instances.push_back(rightRefl);
    }

    void Upload(const Shader& shader) const
    {
        shader.Use();
        glUniform1i(shader.Uniform("entradaSpotLightCount"), static_cast<int>(instances.size()));
        for (size_t i = 0; i < instances.size(); ++i)
        {
            const auto& inst = instances[i];
            const std::string prefix = "entradaSpotLights[" + std::to_string(i) + "].";
            glUniform3fv(shader.Uniform(prefix + "position"), 1, &inst.spotPos[0]);
            glUniform3fv(shader.Uniform(prefix + "direction"), 1, &inst.spotDir[0]);
            glUniform3fv(shader.Uniform(prefix + "color"), 1, &inst.spotColor[0]);
            glUniform1f(shader.Uniform(prefix + "range"), inst.range);
            glUniform1f(shader.Uniform(prefix + "innerCutoff"), inst.innerCutoff);
            glUniform1f(shader.Uniform(prefix + "outerCutoff"), inst.outerCutoff);
        }
    }

    void Draw(const Model& model, const Shader& shader, const glm::mat4& view, const glm::mat4& projection) const
    {
        shader.Use();
        for (const auto& inst : instances)
        {
            model.Draw(shader, projection * view * inst.modelMatrix, true, inst.modelMatrix);
        }
    }

    void Validate() const
    {
        if (instances.size() != 2)
            throw std::runtime_error("Se requieren exactamente dos reflectores en la entrada.");
        for (size_t i = 0; i < instances.size(); ++i)
        {
            const auto& inst = instances[i];
            if (!std::isfinite(inst.range) || inst.range <= 0.0f)
                throw std::runtime_error("Rango invalido en reflector de entrada.");
            if (std::abs(glm::length(inst.spotDir) - 1.0f) > 0.01f)
                throw std::runtime_error("Direccion no normalizada en reflector de entrada.");
            if (inst.innerCutoff <= inst.outerCutoff)
                throw std::runtime_error("Cutoffs invalidos en reflector de entrada.");
            if (std::abs(inst.pitch) > 0.001f)
                throw std::runtime_error("El reflector debe permanecer plano en el suelo (pitch = 0).");
        }
        std::cout << "Entrada: dos reflectores planos en el suelo OK, iluminando letrero y ajolote." << std::endl;
    }
};
