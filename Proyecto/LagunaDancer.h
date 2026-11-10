#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "MovementSettings.h"

class LagunaDancer
{
public:
    void Configure(const Model& environment, Model& dancer,
        const glm::vec3& sourceOrigin, float worldScale)
    {
        const auto* platform = environment.GetMaterialPositions("Material.027");
        if (!platform || platform->empty())
            throw std::runtime_error("No se encontro la geometria de Tarima.obj.");

        // El escenario es la plataforma rectangular al final de la saliente larga
        // de Tarima.obj. En las coordenadas de EntornoCompleto queda en este
        // rectangulo; la esquina opuesta (x≈297, z≈31) es una plataforma lateral.
        glm::vec3 lo((std::numeric_limits<float>::max)());
        glm::vec3 hi((std::numeric_limits<float>::lowest)());
        size_t samples = 0;
        for (const auto& p : *platform)
        {
            if (p.x < 140.0f || p.x > 245.0f || p.z < -120.0f || p.z > 40.0f) continue;
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
            ++samples;
        }
        if (samples < 4 || hi.x - lo.x < 20.0f || hi.z - lo.z < 15.0f)
            throw std::runtime_error("No se pudo determinar el centro del escenario de la laguna.");

        const glm::vec3 center((lo.x + hi.x) * 0.5f, hi.y, (lo.z + hi.z) * 0.5f);
        position = (center - sourceOrigin) * worldScale;
        const float modelHeight = dancer.GetMaxBounds().y - dancer.GetMinBounds().y;
        if (modelHeight <= 0.001f)
            throw std::runtime_error("XolotlDanza.fbx no tiene altura valida.");
        constexpr float characterScale = 5.0f;
        modelScale = MovementSettings::CharacterHeight * worldScale * 1.12f * characterScale / modelHeight;
        if (!dancer.HasBakedAnimation())
            throw std::runtime_error("XolotlDanza.xanim no contiene poses para reproducir.");
        std::cout << "Xolotl en el centro del escenario: " << center.x << ", " << center.y << ", "
            << center.z << "; 17 huesos, escala 5.0f (5.6x la nina), giro horario 90 grados, baile ciclico activo." << std::endl;
        configured = true;
    }

    void Update(Model& dancer, float dt)
    {
        if (!configured) return;
        elapsed += (std::max)(dt, 0.0f);
        dancer.UpdateBakedAnimation(elapsed);
    }

    glm::mat4 ModelMatrix(const Model& dancer) const
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(modelScale));
        return glm::translate(model, glm::vec3(0.0f, -dancer.GetMinBounds().y, 0.0f));
    }

    glm::vec3 ValidationTarget(const Model& dancer) const
    {
        return position + glm::vec3(0.0f, (dancer.GetMaxBounds().y - dancer.GetMinBounds().y) * modelScale * 0.52f, 0.0f);
    }
    glm::vec3 ValidationEye(const Model& dancer) const
    {
        return ValidationTarget(dancer) + glm::vec3(35.0f, 12.0f, 85.0f);
    }

    bool Configured() const { return configured; }

private:
    glm::vec3 position = glm::vec3(0.0f);
    float modelScale = 1.0f;
    float elapsed = 0.0f;
    bool configured = false;
};
