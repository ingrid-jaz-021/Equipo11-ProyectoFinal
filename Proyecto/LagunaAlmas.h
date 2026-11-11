#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "MovementSettings.h"
#include "LagunaSpotLighting.h"

class LagunaAlmas
{
public:
    static constexpr size_t Count = 5;
    static constexpr size_t RowCount = 2;
    static constexpr float Pi = 3.14159265358979323846f;

    void Configure(const Model& environment, const Model& alma,
        const glm::vec3& sourceOrigin, float worldScale)
    {
        const auto* platform = environment.GetMaterialPositions("Material.027");
        if (!platform || platform->empty())
            throw std::runtime_error("No se encontro la plataforma principal de la laguna.");

        glm::vec3 pyramidMin, pyramidMax;
        if (!environment.GetMaterialBounds("Material.025", pyramidMin, pyramidMax))
            throw std::runtime_error("No se encontro Piramide2 para colocar las almas.");
        const auto* pyramid = environment.GetMaterialPositions("Material.025");
        if (!pyramid || pyramid->empty())
            throw std::runtime_error("No se encontro el frente de Piramide2 para orientar los recorridos.");

        float platformTop = (std::numeric_limits<float>::lowest)();
        for (const auto& point : *platform) platformTop = (std::max)(platformTop, point.y);

        glm::vec3 baseLeft((std::numeric_limits<float>::max)());
        glm::vec3 baseRight((std::numeric_limits<float>::max)());
        const float baseY = pyramidMin.y + 1.0f;
        const float splitX = (pyramidMin.x + pyramidMax.x) * 0.5f;
        for (const auto& point : *pyramid)
        {
            if (point.y > baseY) continue;
            if (point.x < splitX && point.z < baseLeft.z) baseLeft = point;
            if (point.x >= splitX && point.z < baseRight.z) baseRight = point;
        }
        if (baseLeft.x == (std::numeric_limits<float>::max)() ||
            baseRight.x == (std::numeric_limits<float>::max)() || baseRight.x - baseLeft.x < 20.0f)
            throw std::runtime_error("No se pudo determinar el frente de Piramide2.");

        const glm::vec2 leftXZ(baseLeft.x, baseLeft.z);
        const glm::vec2 rightXZ(baseRight.x, baseRight.z);
        const glm::vec2 frontCenter = (leftXZ + rightXZ) * 0.5f;
        const glm::vec2 frontAxis = glm::normalize(rightXZ - leftXZ);
        glm::vec2 frontSide(-frontAxis.y, frontAxis.x);
        const glm::vec2 pyramidCenter((pyramidMin.x + pyramidMax.x) * 0.5f,
            (pyramidMin.z + pyramidMax.z) * 0.5f);
        if (glm::dot(frontSide, pyramidCenter - frontCenter) < 0.0f) frontSide = -frontSide;

        const auto lamps = LagunaSpotLighting::FindLampCenters(environment);
        struct BaseLamp
        {
            float side;
            glm::vec3 center;
        };
        std::vector<BaseLamp> baseLamps;
        for (const auto& lamp : lamps)
        {
            // Las cuatro luces de la base principal quedan junto al frente de
            // Piramide2; las demas pertenecen a otras plataformas de la laguna.
            if (lamp.x < pyramidMin.x - 25.0f || lamp.x > pyramidMax.x + 15.0f ||
                lamp.z < pyramidMin.z - 15.0f || lamp.z > pyramidMax.z + 15.0f)
                continue;
            baseLamps.push_back({ glm::dot(glm::vec2(lamp.x, lamp.z) - frontCenter, frontSide), lamp });
        }
        if (baseLamps.size() != RowCount * 2)
            throw std::runtime_error("No se pudieron localizar los dos pares de LuzEsc de la base principal.");
        std::sort(baseLamps.begin(), baseLamps.end(),
            [](const BaseLamp& a, const BaseLamp& b) { return a.side < b.side; });

        const float modelHeight = alma.GetMaxBounds().y - alma.GetMinBounds().y;
        if (modelHeight <= 0.001f)
            throw std::runtime_error("alma.obj no tiene altura valida.");
        constexpr float sizeFactor = 1.25f;
        characterHeight = MovementSettings::CharacterHeight * worldScale * sizeFactor;
        modelScale = characterHeight / modelHeight;
        spacing = characterHeight * 0.80f;

        const auto& almaLo = alma.GetMinBounds();
        const auto& almaHi = alma.GetMaxBounds();
        const float almaRadiusSource = 0.5f * glm::length(glm::vec2(almaHi.x - almaLo.x,
            almaHi.z - almaLo.z)) * modelScale / worldScale;
        constexpr float lampRadiusSource = 1.4f;
        constexpr float extraClearanceSource = 0.25f;
        constexpr float turnRadiusSource = 1.7f;
        for (size_t row = 0; row < RowCount; ++row)
        {
            const auto& a = baseLamps[row * 2].center;
            const auto& b = baseLamps[row * 2 + 1].center;
            ConfigureRoute(routes[row], a, b, lamps, sourceOrigin, worldScale, platformTop,
                pyramidMin, pyramidMax, leftXZ, rightXZ, pyramidCenter, almaRadiusSource,
                lampRadiusSource, extraClearanceSource, turnRadiusSource);
        }
        configured = true;

        std::cout << "Laguna: dos filas de cinco almas, escala x" << sizeFactor
            << "; diametro de giro " << turnRadiusSource * 2.0f << " unidades fuente."
            << std::endl;
    }

    void Update(float dt)
    {
        if (!configured) return;
        for (auto& route : routes)
            route.phase = std::fmod(route.phase + speed * (std::max)(dt, 0.0f), route.trackLength);
    }

    glm::mat4 ModelMatrix(const Model& alma, size_t index, size_t row) const
    {
        if (!configured || index >= Count || row >= RowCount) return glm::mat4(1.0f);
        const auto& route = routes[row];
        const auto pose = PathPose(route, route.phase - spacing * static_cast<float>(index));
        const float yaw = std::atan2(pose.tangent.x, pose.tangent.z) - route.headingCorrection;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), pose.position);
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(modelScale));
        const auto& lo = alma.GetMinBounds();
        const auto& hi = alma.GetMaxBounds();
        const glm::vec3 localCenter((lo.x + hi.x) * 0.5f, lo.y, (lo.z + hi.z) * 0.5f);
        return glm::translate(model, -localCenter);
    }

    glm::vec3 Position(size_t index, size_t row) const
    {
        if (!configured || index >= Count || row >= RowCount) return glm::vec3(0.0f);
        const auto& route = routes[row];
        return PathPose(route, route.phase - spacing * static_cast<float>(index)).position;
    }

    glm::vec3 ValidationTarget(size_t row) const
    {
        if (!configured || row >= RowCount) return glm::vec3(0.0f);
        glm::vec3 target(0.0f);
        for (size_t i = 0; i < Count; ++i) target += Position(i, row);
        return target / static_cast<float>(Count) + glm::vec3(0.0f, characterHeight * 0.5f, 0.0f);
    }

    glm::vec3 ValidationEye(size_t row) const
    {
        return ValidationTarget(row) + glm::vec3(0.0f, 22.0f, -55.0f);
    }

    void Validate() const
    {
        if (!configured || spacing <= 0.0f)
            throw std::runtime_error("Las dos filas de almas no quedaron configuradas.");
        for (size_t row = 0; row < RowCount; ++row)
        {
            const auto& route = routes[row];
            if (route.trackLength <= 0.0f || route.gapHalfLength <= route.objectClearance)
                throw std::runtime_error("Un recorrido entre LuzEsc no quedo configurado.");
            const float gapExtent = route.gapHalfLength - route.objectClearance;
            for (size_t i = 0; i < Count; ++i)
            {
                const auto p = Position(i, row);
                const glm::vec2 relative(p.x - route.center.x, p.z - route.center.z);
                const float along = glm::dot(relative, route.axis);
                const float across = glm::dot(relative, route.side);
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                    std::abs(along) > gapExtent + 0.001f ||
                    std::abs(p.y - route.center.y) > 0.001f || std::abs(across) > route.turnRadius + 0.001f)
                    throw std::runtime_error("Una fila de almas salio del tramo entre sus dos LuzEsc.");

                const float dx = p.x < route.pyramidMin.x ? route.pyramidMin.x - p.x :
                    (p.x > route.pyramidMax.x ? p.x - route.pyramidMax.x : 0.0f);
                const float dz = p.z < route.pyramidMin.y ? route.pyramidMin.y - p.z :
                    (p.z > route.pyramidMax.y ? p.z - route.pyramidMax.y : 0.0f);
                if (std::sqrt(dx * dx + dz * dz) < route.pyramidClearance)
                    throw std::runtime_error("Una alma se acerco demasiado a Piramide2.");
                for (const auto& lamp : route.lampPositions)
                    if (glm::length(glm::vec2(p.x, p.z) - lamp) < route.lampClearance)
                        throw std::runtime_error("Una alma se acerco demasiado a una instancia de LuzEsc.");
                if (i && glm::length(Position(i - 1, row) - p) < spacing * 0.85f)
                    throw std::runtime_error("Dos almas de la misma fila se encimaron durante el recorrido.");
            }
        }
        for (size_t first = 0; first < Count; ++first)
            for (size_t second = 0; second < Count; ++second)
                if (glm::length(Position(first, 0) - Position(second, 1)) < spacing * 0.85f)
                    throw std::runtime_error("Las dos filas de almas se encimaron.");
        std::cout << "Laguna: dos filas de cinco almas van y regresan entre cuatro LuzEsc; sin choques con la piramide ni las luces." << std::endl;
    }

    bool Configured() const { return configured; }

private:
    struct Route
    {
        glm::vec3 center = glm::vec3(0.0f);
        glm::vec2 axis = glm::vec2(1.0f, 0.0f);
        glm::vec2 side = glm::vec2(0.0f, 1.0f);
        glm::vec2 pyramidMin = glm::vec2(0.0f);
        glm::vec2 pyramidMax = glm::vec2(0.0f);
        std::vector<glm::vec2> lampPositions;
        float headingCorrection = 0.0f;
        float halfStraight = 0.0f;
        float turnRadius = 0.0f;
        float gapHalfLength = 0.0f;
        float objectClearance = 0.0f;
        float lampClearance = 0.0f;
        float pyramidClearance = 0.0f;
        float trackLength = 0.0f;
        float phase = 0.0f;
    };

    struct Pose
    {
        glm::vec3 position;
        glm::vec3 tangent;
    };

    void ConfigureRoute(Route& route, const glm::vec3& lightA, const glm::vec3& lightB,
        const std::vector<glm::vec3>& lamps, const glm::vec3& sourceOrigin, float worldScale,
        float platformTop, const glm::vec3& pyramidMin, const glm::vec3& pyramidMax,
        const glm::vec2& frontA, const glm::vec2& frontB, const glm::vec2& pyramidCenter,
        float almaRadiusSource, float lampRadiusSource, float extraClearanceSource,
        float turnRadiusSource)
    {
        const glm::vec2 a(lightA.x, lightA.z);
        const glm::vec2 b(lightB.x, lightB.z);
        const float lampDistanceSource = glm::length(b - a);
        if (lampDistanceSource < 12.0f || lampDistanceSource > 35.0f)
            throw std::runtime_error("Un par de LuzEsc no delimita un tramo seguro para las almas.");

        route.axis = glm::normalize(b - a);
        route.side = glm::vec2(-route.axis.y, route.axis.x);
        const glm::vec2 sourceXZ = (a + b) * 0.5f;
        if (glm::dot(route.side, pyramidCenter - sourceXZ) < 0.0f) route.side = -route.side;
        route.center = (glm::vec3(sourceXZ.x, platformTop, sourceXZ.y) - sourceOrigin) * worldScale;
        route.headingCorrection = std::atan2(route.axis.x, route.axis.y) - glm::radians(90.0f);

        const float objectClearanceSource = lampRadiusSource + almaRadiusSource + extraClearanceSource;
        const float halfStraightSource = lampDistanceSource * 0.5f - objectClearanceSource - turnRadiusSource;
        if (halfStraightSource * 2.0f < spacing / worldScale * static_cast<float>(Count - 1))
            throw std::runtime_error("El espacio entre LuzEsc no permite colocar la fila de cinco almas.");

        route.halfStraight = halfStraightSource * worldScale;
        route.turnRadius = turnRadiusSource * worldScale;
        route.gapHalfLength = lampDistanceSource * 0.5f * worldScale;
        route.objectClearance = objectClearanceSource * worldScale;
        route.lampClearance = (lampRadiusSource + almaRadiusSource) * worldScale;
        route.pyramidClearance = (almaRadiusSource + extraClearanceSource) * worldScale;
        route.pyramidMin = glm::vec2((pyramidMin.x - sourceOrigin.x) * worldScale,
            (pyramidMin.z - sourceOrigin.z) * worldScale);
        route.pyramidMax = glm::vec2((pyramidMax.x - sourceOrigin.x) * worldScale,
            (pyramidMax.z - sourceOrigin.z) * worldScale);
        route.lampPositions.clear();
        route.lampPositions.reserve(lamps.size());
        for (const auto& lamp : lamps)
            route.lampPositions.emplace_back((lamp.x - sourceOrigin.x) * worldScale,
                (lamp.z - sourceOrigin.z) * worldScale);
        route.trackLength = 4.0f * route.halfStraight + 2.0f * Pi * route.turnRadius;
        route.phase = route.halfStraight;

        const glm::vec2 frontAWorld((frontA.x - sourceOrigin.x) * worldScale,
            (frontA.y - sourceOrigin.z) * worldScale);
        const glm::vec2 frontBWorld((frontB.x - sourceOrigin.x) * worldScale,
            (frontB.y - sourceOrigin.z) * worldScale);
        if (DistanceToSegment(glm::vec2(route.center.x, route.center.z), frontAWorld, frontBWorld) <
            route.pyramidClearance)
            throw std::runtime_error("El recorrido quedo demasiado cerca del frente de Piramide2.");
    }

    Pose PathPose(const Route& route, float value) const
    {
        value = std::fmod(value, route.trackLength);
        if (value < 0.0f) value += route.trackLength;
        const float straightLength = 2.0f * route.halfStraight;
        const float capLength = Pi * route.turnRadius;
        if (value < straightLength)
            return { route.center + TrackOffset3(route, value - route.halfStraight, route.turnRadius),
                TrackDirection(route, 1.0f, 0.0f) };
        value -= straightLength;
        if (value < capLength)
        {
            const float angle = Pi * 0.5f - value / route.turnRadius;
            return { route.center + TrackOffset3(route,
                    route.halfStraight + route.turnRadius * std::cos(angle),
                    route.turnRadius * std::sin(angle)),
                TrackDirection(route, std::sin(angle), -std::cos(angle)) };
        }
        value -= capLength;
        if (value < straightLength)
            return { route.center + TrackOffset3(route, route.halfStraight - value, -route.turnRadius),
                TrackDirection(route, -1.0f, 0.0f) };
        value -= straightLength;
        const float angle = -Pi * 0.5f - value / route.turnRadius;
        return { route.center + TrackOffset3(route,
                -route.halfStraight + route.turnRadius * std::cos(angle),
                route.turnRadius * std::sin(angle)),
            TrackDirection(route, std::sin(angle), -std::cos(angle)) };
    }

    static glm::vec2 TrackOffset(const Route& route, float along, float across)
    {
        return glm::vec2(route.axis.x * along + route.side.x * across,
            route.axis.y * along + route.side.y * across);
    }

    static glm::vec3 TrackOffset3(const Route& route, float along, float across)
    {
        const auto offset = TrackOffset(route, along, across);
        return glm::vec3(offset.x, 0.0f, offset.y);
    }

    static glm::vec3 TrackDirection(const Route& route, float along, float across)
    {
        const auto direction = TrackOffset(route, along, across);
        return glm::vec3(direction.x, 0.0f, direction.y);
    }

    static float DistanceToSegment(const glm::vec2& point, const glm::vec2& a, const glm::vec2& b)
    {
        const glm::vec2 segment = b - a;
        const float lengthSquared = glm::dot(segment, segment);
        const float t = lengthSquared > 0.0f
            ? glm::clamp(glm::dot(point - a, segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;
        return glm::length(point - (a + segment * t));
    }

    std::array<Route, RowCount> routes{};
    float modelScale = 1.0f;
    float characterHeight = 0.0f;
    float spacing = 0.0f;
    float speed = 24.0f;
    bool configured = false;
};
