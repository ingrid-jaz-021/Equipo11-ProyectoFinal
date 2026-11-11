#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "Shader.h"
#include "GroundSurface.h"

class FuegoCamino
{
public:
    static constexpr float ModelScale = 0.38f;

    struct LightPoint
    {
        glm::vec3 worldPosition;
        float flickerPhase;
        float flickerFreq;
    };

    struct LightUniformNames
    {
        std::string posName;
        std::string colName;
        std::string rangeName;
    };

    void Build(const GroundSurface* ground, const glm::vec3& sourceOrigin, float worldScale)
    {
        origin = sourceOrigin;
        scale = worldScale;

        // Posiciones locales a lo largo del pasillo de tapetes de aserrin,
        // situadas sobre la banqueta exactamente entre las flores de cempasuchil
        // como se especifica en la referencia visual.
        static const glm::vec2 localCoords[] = {
            // 1. Acera izquierda del pasillo principal (X ≈ -124.78)
            { -124.78f,  223.24f },
            { -124.78f,  192.53f },
            { -124.78f,  161.82f },
            { -124.78f,  126.30f },
            { -124.78f,   88.63f },
            { -124.78f,   53.30f },
            { -124.78f,   17.21f },
            { -124.78f,  -20.20f },

            // 2. Acera derecha del pasillo principal (X ≈ -105.08)
            { -105.08f,  223.24f },
            { -105.08f,  192.53f },
            { -105.08f,  161.82f },
            { -105.08f,  126.30f },
            { -105.08f,   88.63f },
            { -105.08f,   53.30f },
            { -105.08f,   17.21f },

            // 3. Acera superior del pasillo transversal hacia el lago (Z ≈ -19.48)
            {  -87.14f,  -19.48f },
            {  -51.67f,  -19.48f },
            {  -16.36f,  -19.48f },
            {   19.13f,  -19.48f },

            // 4. Acera inferior del pasillo transversal hacia el lago (Z ≈ -39.83)
            {  -86.97f,  -39.83f },
            {  -51.51f,  -39.83f },
            {  -16.19f,  -39.83f },
            {   19.30f,  -39.83f }
        };

        modelMatrices.clear();
        worldPositions.clear();
        lights.clear();
        uniformNames.clear();

        constexpr float bottomOffset = 0.412f * ModelScale; // Compensar base del modelo sobre la banqueta

        float minX = 1e9f, maxX = -1e9f;
        float minZ = 1e9f, maxZ = -1e9f;

        for (size_t i = 0; i < sizeof(localCoords) / sizeof(localCoords[0]); ++i)
        {
            const auto& coord = localCoords[i];
            float surfaceY = 1.776f;
            if (ground)
            {
                float sampledY = 0.0f;
                if (ground->SampleCharacter(coord.x, coord.y, 1.8f, sampledY))
                    surfaceY = sampledY;
            }

            const glm::vec3 localPos(coord.x, surfaceY + bottomOffset, coord.y);
            const glm::vec3 worldPos = (localPos - origin) * scale;
            worldPositions.push_back(worldPos);

            glm::mat4 m = glm::translate(glm::mat4(1.0f), worldPos);
            m = glm::scale(m, glm::vec3(scale * ModelScale));
            modelMatrices.push_back(m);

            // Fuente de luz puntual situada en el corazon de la llama del brasero
            const glm::vec3 flameWorldPos = worldPos + glm::vec3(0.0f, 1.8f * ModelScale * scale, 0.0f);
            const float phase = static_cast<float>(i) * 1.37f;
            const float freq = 13.5f + static_cast<float>(i % 5) * 1.8f;
            lights.push_back({ flameWorldPos, phase, freq });

            minX = std::min(minX, flameWorldPos.x);
            maxX = std::max(maxX, flameWorldPos.x);
            minZ = std::min(minZ, flameWorldPos.z);
            maxZ = std::max(maxZ, flameWorldPos.z);

            const std::string prefix = "fuegoCaminoLights[" + std::to_string(i) + "].";
            uniformNames.push_back({ prefix + "position", prefix + "color", prefix + "range" });
        }

        constexpr float margin = 70.0f;
        lightRegion = glm::vec4(minX - margin, minZ - margin, maxX + margin, maxZ + margin);
        configured = !modelMatrices.empty();
    }

    // Actualiza las luces puntuales calidas de cada fuego con parpadeo organico asincrono
    void UploadLights(const Shader& shader, float now) const
    {
        if (!configured || lights.empty()) return;
        shader.Use();

        glUniform1i(shader.Uniform("fuegoCaminoLightCount"), static_cast<int>(lights.size()));
        glUniform4fv(shader.Uniform("fuegoCaminoRegion"), 1, &lightRegion[0]);

        // Color fuego calido (ambar/dorado, identico a las antorchas)
        const glm::vec3 baseFireColor(1.0f, 0.58f, 0.16f);
        constexpr float range = 42.0f; // Alcance para iluminar el brasero, la banqueta y flores cercanas

        for (size_t i = 0; i < lights.size(); ++i)
        {
            const float ph = lights[i].flickerPhase;
            const float fr = lights[i].flickerFreq;
            const float flicker = 0.13f * std::sin(now * fr + ph)
                                + 0.08f * std::sin(now * (fr * 1.83f) + ph * 2.1f)
                                + 0.04f * std::cos(now * (fr * 2.71f) + ph * 0.7f);

            const glm::vec3 color = baseFireColor * (1.65f + flicker);

            glUniform3fv(shader.Uniform(uniformNames[i].posName), 1, &lights[i].worldPosition[0]);
            glUniform3fv(shader.Uniform(uniformNames[i].colName), 1, &color[0]);
            glUniform1f(shader.Uniform(uniformNames[i].rangeName), range);
        }
    }

    void Validate() const
    {
        if (modelMatrices.size() != 23 || lights.size() != 23)
            throw std::runtime_error("Numero incorrecto de canastas de fuego o fuentes de luz del camino.");
        std::cout << "Fuego Camino: " << modelMatrices.size() << " braseros con fuentes de luz puntual calida OK.\n";
    }

    bool Configured() const { return configured; }
    size_t Count() const { return modelMatrices.size(); }
    const std::vector<glm::mat4>& Matrices() const { return modelMatrices; }
    const std::vector<glm::vec3>& WorldPositions() const { return worldPositions; }
    const std::vector<LightPoint>& Lights() const { return lights; }

private:
    std::vector<glm::mat4> modelMatrices;
    std::vector<glm::vec3> worldPositions;
    std::vector<LightPoint> lights;
    std::vector<LightUniformNames> uniformNames;
    glm::vec4 lightRegion = glm::vec4(0.0f);
    glm::vec3 origin = glm::vec3(0.0f);
    float scale = 1.0f;
    bool configured = false;
};
