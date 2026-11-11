#pragma once
#include <iostream>
#include <vector>
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

        constexpr float bottomOffset = 0.412f * ModelScale; // Compensar base del modelo sobre la banqueta

        for (const auto& coord : localCoords)
        {
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
        }

        configured = !modelMatrices.empty();
    }

    void Validate() const
    {
        if (modelMatrices.size() != 23)
            throw std::runtime_error("Numero incorrecto de canastas de fuego del camino.");
        std::cout << "Fuego Camino: " << modelMatrices.size() << " braseros colocados entre flores a lo largo del pasillo OK.\n";
    }

    bool Configured() const { return configured; }
    size_t Count() const { return modelMatrices.size(); }
    const std::vector<glm::mat4>& Matrices() const { return modelMatrices; }
    const std::vector<glm::vec3>& WorldPositions() const { return worldPositions; }

private:
    std::vector<glm::mat4> modelMatrices;
    std::vector<glm::vec3> worldPositions;
    glm::vec3 origin = glm::vec3(0.0f);
    float scale = 1.0f;
    bool configured = false;
};

