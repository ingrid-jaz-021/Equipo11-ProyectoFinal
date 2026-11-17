#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include "Skinning.h"

// Five bones in the original OBJ space: fixed torso, thigh and calf per leg.
// Bind vertices, UVs, clothing and the separate carrying arms remain unchanged.
struct GuiaLegRig
{
    enum class Body { None, Hombre, Mujer };
    Body body=Body::None;
    float Center() const{return body==Body::Hombre?-19.37f:11.21f;}
    glm::vec3 Hip(int side) const
    {return {Center()+(side?1:-1)*(body==Body::Hombre?1.02f:.88f),body==Body::Hombre?8.25f:8.0f,body==Body::Hombre?-5.4f:-2.7f};}
    glm::vec3 Knee(int side) const
    {const auto h=Hip(side);return {h.x,body==Body::Hombre?4.65f:4.55f,body==Body::Hombre?-5.65f:-3.0f};}
    SkinVertex Weights(const glm::vec3& p) const
    {
        const int side=p.x>Center()?1:0,thigh=1+side*2;
        const float leg=(1-glm::smoothstep(Hip(side).y-.7f,Hip(side).y+.6f,p.y))*
            glm::smoothstep(.12f,.65f,std::abs(p.x-Center()));
        const float calf=1-glm::smoothstep(Knee(side).y-.55f,Knee(side).y+.55f,p.y);
        SkinVertex w;w.joints={0,thigh,thigh+1,0};w.weights={1-leg,leg*(1-calf),leg*calf,0};return w;
    }
    static glm::vec2 Angles(float phase,float amount)
    {
        const float swing=std::max(0.0f,std::sin(phase-.35f));
        return glm::radians(glm::vec2(-25*std::sin(phase),5+48*swing*swing))*amount;
    }
    std::vector<glm::mat4> Palette(float phase,float amount) const
    {
        std::vector<glm::mat4> bones(5,glm::mat4(1));
        for(int side=0;side<2;++side)
        {
            const auto hip=Hip(side),knee=Knee(side);const auto a=Angles(phase+side*3.14159265f,amount);
            const auto upper=glm::translate(glm::mat4(1),hip)*glm::rotate(glm::mat4(1),a.x,glm::vec3(1,0,0));
            bones[1+side*2]=glm::translate(upper,-hip);
            bones[2+side*2]=glm::translate(glm::rotate(glm::translate(upper,knee-hip),a.y,glm::vec3(1,0,0)),-knee);
        }
        return bones;
    }
};
