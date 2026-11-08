#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "MovementSettings.h"

class ZocaloLighting
{
public:
    struct Instance
    {
        glm::mat4 model;
        glm::vec3 base;
        glm::vec3 light;
    };
    static constexpr size_t Count=12;
    std::array<Instance, Count> instances;

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
        if(!environment)
        {
            const glm::vec3 anchor((kioskMin.x+kioskMax.x)*.5f,floorMax.y,(kioskMin.z+kioskMax.z)*.5f);
            kioskMin=anchor+(kioskMin-anchor)*MovementSettings::KioskScale;
            kioskMax=anchor+(kioskMax-anchor)*MovementSettings::KioskScale;
        }
        glm::vec3 yellowMin(-184.399033f,0,261.558807f),yellowMax(-43.978149f,2.038685f,401.612457f);
        glm::vec3 borderMin(-195.836182f,0,250.082703f),borderMax(-32.048828f,2.038686f,412.847565f);
        if(environment)
        {
            yellowMin=glm::vec3((std::numeric_limits<float>::max)());yellowMax=glm::vec3((std::numeric_limits<float>::lowest)());
            for(const char* name:{"Material.016","Material.017","Material.018","Material.019"})
            {
                glm::vec3 lo,hi;
                if(!environment->GetMaterialBounds(name,lo,hi))throw std::runtime_error("Falta la linea amarilla perimetral.");
                yellowMin=glm::min(yellowMin,lo);yellowMax=glm::max(yellowMax,hi);
            }
            if(!environment->GetMaterialBounds("Material.020",borderMin,borderMax))throw std::runtime_error("Falta la calle exterior.");
        }
        glm::vec3 glassMin, glassMax;
        if (!lamp.GetMaterialBounds("Material.002", glassMin, glassMax))
            throw std::runtime_error("La farola no contiene su material de cristal.");
        // Medido sobre la malla real: centro local (0.397158, 1.867290, 0).
        // Se vuelve a calcular tras Assimp para respetar cualquier transformacion del OBJ.
        const glm::vec3 bulb = (glassMin + glassMax) * 0.5f;
        const float lampScale = MovementSettings::LampHeight * worldScale / (lamp.GetMaxBounds().y - lamp.GetMinBounds().y);
        // El poste de este export esta centrado en XZ=(0,0), no en el centro del brazo lateral.
        const glm::vec3 footAnchor(0.0f, lamp.GetMinBounds().y, 0.0f);
        glm::vec3 footMin,footMax;
        const float modelHeight=lamp.GetMaxBounds().y-lamp.GetMinBounds().y;
        if(!lamp.GetMaterialSectionBounds("Material.001",1,footAnchor.y,footAnchor.y+modelHeight*.07f,footMin,footMax))
            throw std::runtime_error("Falta el perfil de la base de las farolas.");
        const float footRadius=std::sqrt(std::max(footMin.x*footMin.x,footMax.x*footMax.x)+
            std::max(footMin.z*footMin.z,footMax.z*footMax.z))*lampScale/worldScale;
        const float outside=footRadius+.15f;
        const glm::vec2 center((kioskMin.x + kioskMax.x) * 0.5f, (kioskMin.z + kioskMax.z) * 0.5f);
        const float inset = 4.0f;
        const float radius = 0.5f * std::max(kioskMax.x - kioskMin.x, kioskMax.z - kioskMin.z) + 6.0f;
        const std::array<glm::vec2, Count> sites = {
            glm::vec2(floorMin.x + inset, floorMin.z + inset),
            glm::vec2(floorMax.x - inset, floorMin.z + inset),
            glm::vec2(floorMax.x - inset, floorMax.z - inset),
            glm::vec2(floorMin.x + inset, floorMax.z - inset),
            center + glm::vec2(-radius, 0), center + glm::vec2(radius, 0),
            center + glm::vec2(0, -radius), center + glm::vec2(0, radius),
            glm::vec2(yellowMin.x-outside,yellowMin.z-outside),
            glm::vec2(yellowMax.x+outside,yellowMin.z-outside),
            glm::vec2(yellowMax.x+outside,yellowMax.z+outside),
            glm::vec2(yellowMin.x-outside,yellowMax.z+outside)
        };
        region = glm::vec4((borderMin.x - sourceOrigin.x) * worldScale,
            (borderMin.z - sourceOrigin.z) * worldScale,
            (borderMax.x - sourceOrigin.x) * worldScale,
            (borderMax.z - sourceOrigin.z) * worldScale);
        range = 38.0f * worldScale;
        for (size_t i = 0; i < sites.size(); ++i)
        {
            if (i<8&&(sites[i].x <= floorMin.x || sites[i].x >= floorMax.x ||
                sites[i].y <= floorMin.z || sites[i].y >= floorMax.z))
                throw std::runtime_error("Una farola queda fuera de la plaza.");
            if(i>=8&&(!(sites[i].x+footRadius<yellowMin.x||sites[i].x-footRadius>yellowMax.x)||
                !(sites[i].y+footRadius<yellowMin.z||sites[i].y-footRadius>yellowMax.z)||
                sites[i].x-footRadius<borderMin.x||sites[i].x+footRadius>borderMax.x||
                sites[i].y-footRadius<borderMin.z||sites[i].y+footRadius>borderMax.z))
                throw std::runtime_error("Una farola exterior invade la linea amarilla o sale de la calle.");
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
            // Recalcular los tres offsets con la NUEVA escala. Incluye altura
            // del cristal y brazo lateral; nunca reutiliza la altura antigua.
            const glm::vec3 scaledBulb=(bulb-footAnchor)*lampScale;
            const glm::mat4 rotation=glm::rotate(glm::mat4(1),yaw,glm::vec3(0,1,0));
            instances[i].light=instances[i].base+glm::vec3(rotation*glm::vec4(scaledBulb,0));
            if (instances[i].light.y <= instances[i].base.y ||
                !std::isfinite(instances[i].light.y))
                throw std::runtime_error("Offset del foco invalido.");
        }
    }

    void Validate(const Model& lamp,float worldScale) const
    {
        glm::vec3 lo,hi;
        if(!lamp.GetMaterialBounds("Material.002",lo,hi))throw std::runtime_error("Falta el cristal de las farolas.");
        for(const auto& instance:instances)
        {
            const auto crystalCenter=glm::vec3(instance.model*glm::vec4((lo+hi)*.5f,1));
            const float height=glm::length(glm::vec3(instance.model[1]))*(lamp.GetMaxBounds().y-lamp.GetMinBounds().y);
            if(glm::length(crystalCenter-instance.light)>.001f||std::abs(height-MovementSettings::LampHeight*worldScale)>.001f)
                throw std::runtime_error("La luz no coincide con el cristal de la farola ampliada.");
        }
        std::cout<<"Farolas OK: altura "<<MovementSettings::LampHeight<<", 12 luces (4 exteriores) en el centro del cristal; offset Y "
            <<(instances[0].light.y-instances[0].base.y)/worldScale<<" unidades fuente."<<std::endl;
    }

    void Upload(const Shader& shader) const
    {
        shader.Use();
        glUniform1i(glGetUniformLocation(shader.Program, "pointLightCount"), static_cast<int>(Count));
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
