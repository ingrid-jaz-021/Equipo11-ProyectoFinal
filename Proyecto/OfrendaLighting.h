#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Model.h"
#include "Shader.h"

class OfrendaLighting
{
public:
    struct Point
    {
        glm::vec3 position;
        glm::vec3 baseColor;
        float range;
        float flickerPhase;
        float flickerFreq;
    };

    static constexpr size_t TotalLightCount = 6;
    static constexpr size_t LampCount = 4;
    static constexpr size_t CandleCount = 2;

    void Build(const Model* environment, const glm::vec3& sourceOrigin, float worldScale)
    {
        if (environment)
        {
            glm::vec3 minB, maxB;
            if (!environment->GetMaterialBounds("lampara_cubo.001", minB, maxB) ||
                !environment->GetMaterialBounds("fuego_vela.001", minB, maxB))
            {
                throw std::runtime_error("No se encontraron los materiales de lamparas o velas de la ofrenda.");
            }
        }

        lights.clear();
        lights.reserve(TotalLightCount);

        // 1. Cuatro cubos lampara ubicados en la ofrenda (alrededor de la cruz y calaveras)
        // Posiciones locales medidas en EntornoCompleto.obj:
        // Cubo 0: (-152.17, 3.51, 182.47) -> World: (-381.73, 22.0, -1075.27)
        // Cubo 1: (-152.20, 3.51, 200.85) -> World: (-381.97, 22.0, -891.52)
        // Cubo 2: (-168.62, 3.51, 182.22) -> World: (-546.21, 22.0, -1077.82)
        // Cubo 3: (-168.64, 3.51, 200.49) -> World: (-546.45, 22.0, -895.06)
        const glm::vec3 localCubes[LampCount] = {
            glm::vec3(-152.17f, 4.2f, 182.47f),
            glm::vec3(-152.20f, 4.2f, 200.85f),
            glm::vec3(-168.62f, 4.2f, 182.22f),
            glm::vec3(-168.64f, 4.2f, 200.49f)
        };

        const float lampPhases[LampCount] = { 0.0f, 1.8f, 3.5f, 5.2f };
        for (size_t i = 0; i < LampCount; ++i)
        {
            const glm::vec3 worldPos = (localCubes[i] - sourceOrigin) * worldScale;
            // Luz ambar/naranja calida caracteristica de las lamparas de cubo de cempasuchil
            lights.push_back({ worldPos, glm::vec3(1.20f, 0.65f, 0.18f), 155.0f, lampPhases[i], 7.5f });
        }

        // 2. Dos focos suaves de veladoras en el eje de la cruz central ("muy bajito")
        // Frente de la cruz y escalon central superior
        const glm::vec3 localCandles[CandleCount] = {
            glm::vec3(-160.40f, 3.5f, 178.0f),
            glm::vec3(-160.40f, 5.2f, 197.0f)
        };
        const float candlePhases[CandleCount] = { 0.9f, 2.7f };
        for (size_t i = 0; i < CandleCount; ++i)
        {
            const glm::vec3 worldPos = (localCandles[i] - sourceOrigin) * worldScale;
            // Luz suave dorada de veladora ("muy bajito")
            lights.push_back({ worldPos, glm::vec3(0.42f, 0.24f, 0.07f), 95.0f, candlePhases[i], 12.0f });
        }

        // 3. Region de la ofrenda para el resplandor ambiental tenue de las veladoras
        // Bounds locales de piso_ofrenda: X=[-185.5, -135.2], Z=[157.2, 208.7]
        // Bounds mundo: X=[-715, -212], Z=[-1328, -813] (con margen de transicion suave)
        candleRegion = glm::vec4(
            (-187.0f - sourceOrigin.x) * worldScale,
            ( 155.0f - sourceOrigin.z) * worldScale,
            (-133.0f - sourceOrigin.x) * worldScale,
            ( 210.0f - sourceOrigin.z) * worldScale
        );

        configured = true;
    }

    void Upload(const Shader& shader, float now) const
    {
        if (!configured) return;
        shader.Use();

        // Cargar las 6 luces puntuales (4 lamparas de cubo + 2 focos suaves de veladoras)
        glUniform1i(shader.Uniform("ofrendaLightCount"), static_cast<int>(lights.size()));
        for (size_t i = 0; i < lights.size(); ++i)
        {
            const std::string prefix = "ofrendaLights[" + std::to_string(i) + "].";
            const float flicker = 1.0f + 0.05f * std::sin(now * lights[i].flickerFreq + lights[i].flickerPhase)
                                       + 0.03f * std::cos(now * (lights[i].flickerFreq * 1.7f) + lights[i].flickerPhase * 1.3f);
            const glm::vec3 lightColor = lights[i].baseColor * flicker;

            glUniform3fv(glGetUniformLocation(shader.Program, (prefix + "position").c_str()), 1, &lights[i].position[0]);
            glUniform3fv(glGetUniformLocation(shader.Program, (prefix + "color").c_str()), 1, &lightColor[0]);
            glUniform1f(glGetUniformLocation(shader.Program, (prefix + "range").c_str()), lights[i].range);
        }

        // Region y resplandor ambiental tenue ("muy bajito") de veladoras
        glUniform4fv(shader.Uniform("ofrendaCandleRegion"), 1, &candleRegion[0]);

        const float candleFlicker = 1.0f + 0.07f * std::sin(now * 11.3f) + 0.04f * std::cos(now * 19.7f);
        const glm::vec3 candleColor = glm::vec3(0.16f, 0.09f, 0.03f) * candleFlicker;
        glUniform3fv(shader.Uniform("ofrendaCandleColor"), 1, &candleColor[0]);
    }

    void Validate() const
    {
        if (lights.size() != TotalLightCount)
            throw std::runtime_error("Numero incorrecto de luces de ofrenda.");
        for (const auto& l : lights)
        {
            if (!std::isfinite(l.range) || l.range <= 0.0f || !std::isfinite(l.position.x))
                throw std::runtime_error("Parametro invalido en una luz de ofrenda.");
        }
        std::cout << "Ofrenda: 4 lamparas de cubo naranja y resplandor tenue de veladoras validados OK.\n";
    }

    bool Configured() const { return configured; }

private:
    std::vector<Point> lights;
    glm::vec4 candleRegion = glm::vec4(0.0f);
    bool configured = false;
};

