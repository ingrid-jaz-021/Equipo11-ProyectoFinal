#pragma once
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Model.h"
#include "Shader.h"

class LagunaSpotLighting
{
public:
    struct Spot
    {
        glm::vec3 position;
        glm::vec3 direction;
        float range;
    };

    static std::vector<glm::vec3> FindLampCenters(const Model& environment)
    {
        const auto* lampVertices = environment.GetMaterialPositions("Material.026");
        if (!lampVertices || lampVertices->empty())
            throw std::runtime_error("No se encontraron instancias de LuzEsc en la malla combinada.");

        // Material.026 pertenece a LuzEsc. Agrupar por proximidad XZ recupera cada
        // instancia aunque Assimp haya importado la escena como una sola malla.
        std::map<std::pair<int, int>, glm::vec3> unique;
        for (const auto& p : *lampVertices)
        {
            const auto key = std::make_pair(static_cast<int>(std::lround(p.x * 50.0f)),
                static_cast<int>(std::lround(p.z * 50.0f)));
            auto found = unique.find(key);
            if (found == unique.end()) unique.emplace(key, p);
            else found->second.y = (std::max)(found->second.y, p.y);
        }
        std::vector<glm::vec3> points;
        points.reserve(unique.size());
        for (const auto& item : unique) points.push_back(item.second);

        constexpr float clusterRadius = 2.5f;
        const float clusterRadiusSquared = clusterRadius * clusterRadius;
        std::vector<bool> visited(points.size(), false);
        std::vector<glm::vec3> centers;
        for (size_t seed = 0; seed < points.size(); ++seed)
        {
            if (visited[seed]) continue;
            visited[seed] = true;
            std::vector<size_t> cluster{ seed };
            for (size_t cursor = 0; cursor < cluster.size(); ++cursor)
            {
                const auto current = cluster[cursor];
                for (size_t candidate = 0; candidate < points.size(); ++candidate)
                {
                    if (visited[candidate]) continue;
                    const float dx = points[current].x - points[candidate].x;
                    const float dz = points[current].z - points[candidate].z;
                    if (dx * dx + dz * dz <= clusterRadiusSquared)
                    {
                        visited[candidate] = true;
                        cluster.push_back(candidate);
                    }
                }
            }

            glm::vec2 lo((std::numeric_limits<float>::max)());
            glm::vec2 hi((std::numeric_limits<float>::lowest)());
            float top = (std::numeric_limits<float>::lowest)();
            for (const size_t index : cluster)
            {
                lo = glm::min(lo, glm::vec2(points[index].x, points[index].z));
                hi = glm::max(hi, glm::vec2(points[index].x, points[index].z));
                top = (std::max)(top, points[index].y);
            }
            if (hi.x - lo.x > 3.5f || hi.y - lo.y > 3.5f || cluster.size() < 4) continue;
            centers.emplace_back((lo.x + hi.x) * 0.5f, top, (lo.y + hi.y) * 0.5f);
        }
        return centers;
    }

    void Build(const Model& environment, const glm::vec3& sourceOrigin, float worldScale)
    {
        glm::vec3 pyramidMin, pyramidMax;
        if (!environment.GetMaterialBounds("Material.025", pyramidMin, pyramidMax))
            throw std::runtime_error("No se encontro Piramide2 en la malla combinada.");
        const auto centers = FindLampCenters(environment);
        std::cout << "Material LuzEsc: " << centers.size() << " instancias agrupadas.\n";
        if (centers.empty() || centers.size() > 32)
            throw std::runtime_error("No se pudo separar de forma segura las instancias de LuzEsc.");

        const glm::vec3 target = (pyramidMin + pyramidMax) * 0.5f;
        spots.clear();
        spots.reserve(centers.size());
        for (const auto& source : centers)
        {
            const glm::vec3 delta = target - source;
            const float distance = glm::length(delta);
            if (distance < 0.1f) throw std::runtime_error("Una LuzEsc coincide con el centro de Piramide2.");
            spots.push_back({ (source - sourceOrigin) * worldScale,
                glm::normalize(delta), distance * worldScale * 1.35f });
        }
        targetWorld = (target - sourceOrigin) * worldScale;
    }

    void Upload(const Shader& shader,const FrustumPlanes* planes=nullptr) const
    {
        std::vector<size_t> visible;
        for(size_t i=0;i<spots.size();++i)if(LightSphereVisible(planes,spots[i].position,spots[i].range))visible.push_back(i);
        const auto old=uploaded.find(shader.Program);
        if(old!=uploaded.end()&&old->second==visible)return;
        uploaded[shader.Program]=visible;shader.Use();
        glUniform1i(shader.Uniform("spotLightCount"),static_cast<int>(visible.size()));
        for(size_t slot=0;slot<visible.size();++slot)
        {
            const auto& spot=spots[visible[slot]];const auto prefix="spotLights["+std::to_string(slot)+"].";
            glUniform3fv(shader.Uniform(prefix+"position"),1,&spot.position[0]);
            glUniform3fv(shader.Uniform(prefix+"direction"),1,&spot.direction[0]);
            glUniform3f(shader.Uniform(prefix+"color"),.08f,.42f,3.2f);
            glUniform1f(shader.Uniform(prefix+"range"),spot.range);
            glUniform1f(shader.Uniform(prefix+"innerCutoff"),std::cos(glm::radians(17.0f)));
            glUniform1f(shader.Uniform(prefix+"outerCutoff"),std::cos(glm::radians(31.0f)));
        }
    }

    void Validate() const
    {
        if (spots.empty()) throw std::runtime_error("No hay reflectores azules en la laguna.");
        for (const auto& spot : spots)
            if (!std::isfinite(spot.range) || spot.range <= 0.0f ||
                std::abs(glm::length(spot.direction) - 1.0f) > 0.001f ||
                glm::dot(spot.direction, glm::normalize(targetWorld - spot.position)) < 0.999f)
                throw std::runtime_error("Un reflector de LuzEsc no apunta hacia Piramide2.");
        std::cout << "Laguna: " << spots.size() << " reflectores azules apuntan a Piramide2.\n";
    }

private:
    mutable std::unordered_map<GLuint,std::vector<size_t>> uploaded;
    std::vector<Spot> spots;
    glm::vec3 targetWorld = glm::vec3(0.0f);
};
