#pragma once
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

// Triangulos transitables en coordenadas del OBJ, indexados por una rejilla XZ.
class GroundSurface
{
public:
    void Add(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        const glm::vec3 normal = glm::cross(b - a, c - a);
        const float area2 = glm::length(normal);
        // Excluir paredes, microgeometria y el reverso de los suelos.
        if (area2 < 0.04f || normal.y < area2 * 0.60f) return;
        const auto lo = glm::min(a, glm::min(b, c));
        const auto hi = glm::max(a, glm::max(b, c));
        const unsigned int id = static_cast<unsigned int>(triangles.size());
        triangles.push_back({ a, b, c });
        for (int z = Cell(lo.z); z <= Cell(hi.z); ++z)
            for (int x = Cell(lo.x); x <= Cell(hi.x); ++x) grid[Key(x, z)].push_back(id);
    }

    bool Sample(float x, float z, float currentY, float& height) const
    {
        const auto cell = grid.find(Key(Cell(x), Cell(z)));
        if (cell == grid.end()) return false;
        bool found = false;
        float highest = -1.0e20f;
        for (unsigned int id : cell->second)
        {
            const auto& t = triangles[id];
            const float det = (t.b.z - t.c.z) * (t.a.x - t.c.x) + (t.c.x - t.b.x) * (t.a.z - t.c.z);
            if (std::abs(det) < 1.0e-8f) continue;
            const float u = ((t.b.z - t.c.z) * (x - t.c.x) + (t.c.x - t.b.x) * (z - t.c.z)) / det;
            const float v = ((t.c.z - t.a.z) * (x - t.c.x) + (t.a.x - t.c.x) * (z - t.c.z)) / det;
            if (u < -0.0001f || v < -0.0001f || u + v > 1.0001f) continue;
            const float y = u * t.a.y + v * t.b.y + (1.0f - u - v) * t.c.y;
            if (y <= currentY + 0.40f && y >= currentY - 6.0f && y > highest)
            {
                highest = y;
                found = true;
            }
        }
        if (found) height = highest;
        return found;
    }
private:
    struct Triangle { glm::vec3 a, b, c; };
    std::vector<Triangle> triangles;
    std::unordered_map<uint64_t, std::vector<unsigned int>> grid;
    static int Cell(float value) { return static_cast<int>(std::floor(value / 8.0f)); }
    static uint64_t Key(int x, int z)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(z);
    }
};
