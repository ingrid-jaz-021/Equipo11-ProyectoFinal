#pragma once
#include <string>
#include <glm/glm.hpp>

// Unidades del OBJ (metros), independientes de la escala global del escenario.
inline glm::vec2 ZocaloTileSize(const std::string& material)
{
    if(material=="Material.006") return {2.4f,1.6f}; // 600x400: respetar proporcion de la imagen.
    if(material=="Material.013") return {4.0f,2.6666667f}; // asfalto 1170x780.
    if(material=="Material.016"||material=="Material.017"||material=="Material.018"||material=="Material.019")
        return {1.5f,2.2511f};
    if(material=="Material.020") return {2.0f,3.0015f};
    if(material=="Material.008"||material=="Material.009"||material=="Material.010"||
        material=="Material.011"||material=="Material.012"||material=="Material.015"||
        material=="Material.021"||material=="Material.022") return {1.5f,2.0f};
    return {0.0f,0.0f};
}
inline glm::vec2 ProjectFloorUV(const glm::vec3& position,const glm::vec3& normal,const glm::vec2& tile)
{
    const glm::vec3 n=glm::abs(normal);
    if(n.y>=n.x&&n.y>=n.z) return {position.x/tile.x,-position.z/tile.y};
    if(n.x>=n.z) return {position.z/tile.x,-position.y/tile.y};
    return {position.x/tile.x,-position.y/tile.y};
}
