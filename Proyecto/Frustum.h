#pragma once
#include <array>
#include <glm/glm.hpp>

using FrustumPlanes=std::array<glm::vec4,6>;
inline FrustumPlanes ExtractFrustum(const glm::mat4& clip)
{
    const auto r=glm::transpose(clip);
    FrustumPlanes planes={r[3]+r[0],r[3]-r[0],r[3]+r[1],r[3]-r[1],r[3]+r[2],r[3]-r[2]};
    for(auto& p:planes){const float length=glm::length(glm::vec3(p));if(length>1e-8f)p/=length;}
    return planes;
}
inline bool BoundsVisible(const FrustumPlanes& planes,const glm::vec3& lo,const glm::vec3& hi)
{
    for(const auto& plane:planes)
    {
        const glm::vec3 p(plane.x>=0?hi.x:lo.x,plane.y>=0?hi.y:lo.y,plane.z>=0?hi.z:lo.z);
        if(glm::dot(glm::vec3(plane),p)+plane.w<-.002f)return false;
    }
    return true;
}
inline bool TransformedBoundsVisible(const FrustumPlanes& planes,const glm::vec3& lo,const glm::vec3& hi,const glm::mat4& model)
{
    const auto center=glm::vec3(model*glm::vec4((lo+hi)*.5f,1));const auto half=(hi-lo)*.5f;
    const auto extent=glm::abs(glm::vec3(model[0]))*half.x+glm::abs(glm::vec3(model[1]))*half.y+glm::abs(glm::vec3(model[2]))*half.z;
    return BoundsVisible(planes,center-extent,center+extent);
}

// Una luz de radio finito solo puede afectar fragmentos dentro de esta esfera.
inline bool LightSphereVisible(const FrustumPlanes* planes,const glm::vec3& center,float radius)
{
    if(!planes)return true;
    for(const auto& p:*planes)if(glm::dot(glm::vec3(p),center)+p.w < -radius-.01f)return false;
    return true;
}
