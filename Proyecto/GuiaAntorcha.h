#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "GroundSurface.h"
#include "Model.h"
#include "MovementSettings.h"

class GuiaAntorcha
{
public:
    static constexpr size_t Count = 2;

    struct Pose
    {
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 boundsMin = glm::vec3(0.0f);
        glm::vec3 boundsMax = glm::vec3(1.0f);
        float phase = 0.0f;
        float travelDirection = -1.0f;
    };

    void Configure(const Model& environment, const GroundSurface& ground,
        const Model& woman, const Model& man, const glm::vec3& sourceOrigin, float worldScale)
    {
        if (worldScale <= 0.0f) throw std::runtime_error("Escala invalida para los Guias Antorcha.");

        glm::vec3 entryLo, entryHi, archLo, archHi;
        if (!environment.GetMaterialEndBounds("TapeteAserrin1.002", 2, 1.0f, entryLo, entryHi))
            throw std::runtime_error("No se pudo localizar la entrada de los tapetes de aserrin.");
        if (!environment.GetMaterialBounds("Arco_Ofrenda.001", archLo, archHi))
            throw std::runtime_error("No se encontro el arco floral de bienvenida.");

        glm::vec3 walkwayLo((std::numeric_limits<float>::max)());
        glm::vec3 walkwayHi((std::numeric_limits<float>::lowest)());
        bool foundWalkway = false;
        for (const char* material : {"TapeteAserrin1.002", "TapeteAserrin1.003", "TapeteAserrin2.001"})
        {
            glm::vec3 lo, hi;
            if (!environment.GetMaterialBounds(material, lo, hi)) continue;
            walkwayLo = glm::min(walkwayLo, lo);
            walkwayHi = glm::max(walkwayHi, hi);
            foundWalkway = true;
        }
        if (!foundWalkway || walkwayHi.z - walkwayLo.z < walkwayHi.x - walkwayLo.x)
            throw std::runtime_error("Los tapetes no definen un recorrido longitudinal por el Pasillo.");

        walkwayCenterX = (walkwayLo.x + walkwayHi.x) * 0.5f;
        walkwayMinX = walkwayLo.x;
        walkwayMaxX = walkwayHi.x;
        directionZ = (walkwayLo.z + walkwayHi.z) * 0.5f < entryHi.z ? -1.0f : 1.0f;
        const float targetHeight = MovementSettings::CharacterHeight * worldScale;
        const std::array<const Model*, Count> models = {&woman, &man};
        float greatestDepthSource = 0.0f;
        for (size_t i = 0; i < Count; ++i)
        {
            const auto& model = *models[i];
            const auto& lo = model.GetMinBounds();
            const auto& hi = model.GetMaxBounds();
            const float height = hi.y - lo.y;
            if (height <= 0.001f) throw std::runtime_error("Un modelo GuiaAntorcha no tiene altura valida.");
            figures[i] = {lo, hi, targetHeight / height, targetHeight};
            greatestDepthSource = (std::max)(greatestDepthSource,
                (hi.z - lo.z) * figures[i].scale / worldScale);
        }

        const float halfDepth = greatestDepthSource * 0.5f;
        const float innerArchFace = directionZ < 0.0f ? archLo.z : archHi.z;
        startZ = innerArchFace + directionZ * (halfDepth + 0.35f);
        const float minCenterZ = walkwayLo.z + halfDepth;
        const float maxCenterZ = walkwayHi.z - halfDepth;
        if (minCenterZ >= maxCenterZ)
            throw std::runtime_error("El tramo de tapetes es demasiado corto para los guias.");
        startZ = glm::clamp(startZ, minCenterZ, maxCenterZ);
        endZ = directionZ < 0.0f ? minCenterZ : maxCenterZ;
        routeLength = std::abs(endZ - startZ) * worldScale;
        // En la referencia los modelos quedan lado a lado con una separación
        // entre centros de aproximadamente 0.8 veces su altura.
        lateralSpacing = targetHeight * 0.8f;
        if (routeLength <= targetHeight * 1.5f || (endZ - startZ) * directionZ <= 0.0f)
            throw std::runtime_error("No hay espacio suficiente despues del arco para el recorrido de los guias.");

        this->ground = &ground;
        this->sourceOrigin = sourceOrigin;
        this->worldScale = worldScale;
        floorHint = walkwayHi.y + 5.0f;
        speed = worldScale * MovementSettings::WalkSpeed;
        configured = true;

        std::cout << "Guias Antorcha: Hombre y Mujer listos despues del arco, lado a lado; recorrido de ida y vuelta por los tapetes, "
            << "marcha procedural sin esqueleto." << std::endl;
    }

    void Update(float dt)
    {
        if (configured) elapsed += (std::max)(dt, 0.0f);
    }

    Pose CurrentPose(size_t index) const
    {
        if (!configured || index >= Count) return {};
        const auto& figure = figures[index];
        const float distance = elapsed * speed;
        const float cycleLength = routeLength * 2.0f;
        const float cycleDistance = std::fmod(distance, cycleLength);
        const bool returning = cycleDistance > routeLength;
        const float along = returning ? cycleLength - cycleDistance : cycleDistance;
        const float travelDirection = returning ? -directionZ : directionZ;
        const float sourceZ = startZ + directionZ * along / worldScale;

        float floor = floorHint;
        const float lateral = (index == 0 ? -0.5f : 0.5f) * lateralSpacing;
        const float halfWidth = (figure.boundsMax.x - figure.boundsMin.x) * figure.scale /
            (2.0f * worldScale);
        const float guideX = glm::clamp(walkwayCenterX + lateral / worldScale,
            walkwayMinX + halfWidth, walkwayMaxX - halfWidth);
        ground->SampleCharacter(guideX, sourceZ, floorHint, floor,
            (figure.boundsMax.x - figure.boundsMin.x) * figure.scale / worldScale * 0.12f, 100.0f);
        glm::vec3 position((guideX - sourceOrigin.x) * worldScale,
            (floor - sourceOrigin.y) * worldScale,
            (sourceZ - sourceOrigin.z) * worldScale);

        const float stride = figure.height * MovementSettings::WalkStrideHeightRatio;
        const float phase = std::fmod(distance / stride * 6.28318530718f, 6.28318530718f);
        position.y += std::abs(std::sin(phase * 2.0f)) * figure.height * 0.018f;
        // Las caras de ambos OBJ miran hacia -Z en su espacio local.
        const float yaw = travelDirection < 0.0f ? 0.0f : 3.14159265359f;
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
        matrix = glm::rotate(matrix, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        matrix = glm::scale(matrix, glm::vec3(figure.scale));
        const glm::vec3 localOrigin((figure.boundsMin.x + figure.boundsMax.x) * 0.5f,
            figure.boundsMin.y, (figure.boundsMin.z + figure.boundsMax.z) * 0.5f);
        matrix = glm::translate(matrix, -localOrigin);
        return {matrix, figure.boundsMin, figure.boundsMax, phase, travelDirection};
    }

    void Validate() const
    {
        if (!configured || routeLength <= figures[0].height * 1.5f)
            throw std::runtime_error("Los Guias Antorcha no quedaron configurados sobre los tapetes.");
        std::cout << "Guias Antorcha OK: recorrido libre de ida y vuelta, con marcha alterna." << std::endl;
    }

    bool Configured() const { return configured; }

private:
    struct Figure
    {
        glm::vec3 boundsMin = glm::vec3(0.0f);
        glm::vec3 boundsMax = glm::vec3(1.0f);
        float scale = 1.0f;
        float height = 1.0f;
    };

    std::array<Figure, Count> figures{};
    const GroundSurface* ground = nullptr;
    glm::vec3 sourceOrigin = glm::vec3(0.0f);
    float worldScale = 1.0f;
    float walkwayCenterX = 0.0f;
    float walkwayMinX = 0.0f;
    float walkwayMaxX = 0.0f;
    float startZ = 0.0f;
    float endZ = 0.0f;
    float directionZ = -1.0f;
    float routeLength = 0.0f;
    float lateralSpacing = 0.0f;
    float speed = 0.0f;
    float floorHint = 0.0f;
    float elapsed = 0.0f;
    bool configured = false;
};

