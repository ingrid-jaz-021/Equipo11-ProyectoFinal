#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"

class ZocaloLighting
{
public:
    struct Instance
    {
        glm::mat4 model;
        glm::vec3 base;
        glm::vec3 light;
    };
    std::array<Instance, 8> instances;

    void Build(const Model& lamp, const Model* environment, const GroundSurface& ground,
        const glm::vec3& sourceOrigin, float worldScale)
    {
        glm::vec3 floorMin(-160.188034f, -2.288378f, 285.375977f);
        glm::vec3 floorMax(-68.132690f, 2.005836f, 377.431335f);
        glm::vec3 kioskMin(-123.978775f, 1.881192f, 321.511719f);
        glm::vec3 kioskMax(-103.839218f, 21.353916f, 341.651276f);
        if (environment && (!environment->GetMaterialBounds("Material.006", floorMin, floorMax) ||
            !environment->GetMaterialBounds("Material.014", kioskMin, kioskMax)))
            throw std::runtime_error("No se encontraron el suelo y el quiosco del Zocalo.");
        glm::vec3 glassMin, glassMax;
        if (!lamp.GetMaterialBounds("Material.002", glassMin, glassMax))
            throw std::runtime_error("La farola no contiene su material de cristal.");
        // Medido sobre la malla real: centro local (0.397158, 1.867290, 0).
        // Se vuelve a calcular tras Assimp para respetar cualquier transformacion del OBJ.
        const glm::vec3 bulb = (glassMin + glassMax) * 0.5f;
        const float lampScale = 5.0f * worldScale / (lamp.GetMaxBounds().y - lamp.GetMinBounds().y);
        // El poste de este export esta centrado en XZ=(0,0), no en el centro del brazo lateral.
        const glm::vec3 footAnchor(0.0f, lamp.GetMinBounds().y, 0.0f);
        const glm::vec2 center((kioskMin.x + kioskMax.x) * 0.5f, (kioskMin.z + kioskMax.z) * 0.5f);
        const float inset = 4.0f;
        const float radius = 0.5f * std::max(kioskMax.x - kioskMin.x, kioskMax.z - kioskMin.z) + 6.0f;
        const std::array<glm::vec2, 8> sites = {
            glm::vec2(floorMin.x + inset, floorMin.z + inset),
            glm::vec2(floorMax.x - inset, floorMin.z + inset),
            glm::vec2(floorMax.x - inset, floorMax.z - inset),
            glm::vec2(floorMin.x + inset, floorMax.z - inset),
            center + glm::vec2(-radius, 0), center + glm::vec2(radius, 0),
            center + glm::vec2(0, -radius), center + glm::vec2(0, radius)
        };
        region = glm::vec4((floorMin.x - sourceOrigin.x) * worldScale,
            (floorMin.z - sourceOrigin.z) * worldScale,
            (floorMax.x - sourceOrigin.x) * worldScale,
            (floorMax.z - sourceOrigin.z) * worldScale);
        range = 22.0f * worldScale;
        for (size_t i = 0; i < sites.size(); ++i)
        {
            if (sites[i].x <= floorMin.x || sites[i].x >= floorMax.x ||
                sites[i].y <= floorMin.z || sites[i].y >= floorMax.z)
                throw std::runtime_error("Una farola queda fuera de la plaza.");
            float floor = floorMax.y;
            ground.Sample(sites[i].x, sites[i].y, floorMax.y, floor);
            const glm::vec3 source(sites[i].x, floor, sites[i].y);
            instances[i].base = (source - sourceOrigin) * worldScale;
            const glm::vec2 inward = center - sites[i];
            const float yaw = std::atan2(-inward.y, inward.x);
            glm::mat4 model = glm::translate(glm::mat4(1), instances[i].base);
            model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
            model = glm::scale(model, glm::vec3(lampScale));
            model = glm::translate(model, -footAnchor);
            instances[i].model = model;
            instances[i].light = glm::vec3(model * glm::vec4(bulb, 1.0f));
            if (instances[i].light.y <= instances[i].base.y ||
                !std::isfinite(instances[i].light.y))
                throw std::runtime_error("Offset del foco invalido.");
        }
    }

    void Upload(const Shader& shader) const
    {
        shader.Use();
        glUniform1i(glGetUniformLocation(shader.Program, "pointLightCount"), 8);
        glUniform4fv(glGetUniformLocation(shader.Program, "pointLightRegion"), 1, &region[0]);
        for (size_t i = 0; i < instances.size(); ++i)
        {
            const std::string prefix = "pointLights[" + std::to_string(i) + "].";
            glUniform3fv(glGetUniformLocation(shader.Program, (prefix + "position").c_str()), 1, &instances[i].light[0]);
            glUniform3f(glGetUniformLocation(shader.Program, (prefix + "color").c_str()), 1.25f, 1.80f, 2.80f);
            glUniform1f(glGetUniformLocation(shader.Program, (prefix + "range").c_str()), range);
        }
    }
private:
    glm::vec4 region = glm::vec4(0);
    float range = 220.0f;
};
