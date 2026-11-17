#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "MovementSettings.h"

class LagunaDancer
{
public:
    static constexpr float HeadingDegrees = 270.0f;

    void Configure(const Model& environment, Model& dancer,
        const glm::vec3& sourceOrigin, float worldScale)
    {
        const auto* platform = environment.GetMaterialPositions("Material.027");
        if (!platform || platform->empty())
            throw std::runtime_error("No se encontro la geometria de Tarima.obj.");

        // El escenario es la plataforma rectangular al final de la saliente larga
        // de Tarima.obj. En las coordenadas de EntornoCompleto queda en este
        // rectangulo; la esquina opuesta (x≈297, z≈31) es una plataforma lateral.
        glm::vec3 lo((std::numeric_limits<float>::max)());
        glm::vec3 hi((std::numeric_limits<float>::lowest)());
        size_t samples = 0;
        for (const auto& p : *platform)
        {
            if (p.x < 140.0f || p.x > 245.0f || p.z < -120.0f || p.z > 40.0f) continue;
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
            ++samples;
        }
        if (samples < 4 || hi.x - lo.x < 20.0f || hi.z - lo.z < 15.0f)
            throw std::runtime_error("No se pudo determinar el centro del escenario de la laguna.");

        const glm::vec3 center((lo.x + hi.x) * 0.5f, hi.y, (lo.z + hi.z) * 0.5f);
        position = (center - sourceOrigin) * worldScale;
        const float modelHeight = dancer.GetMaxBounds().y - dancer.GetMinBounds().y;
        if (modelHeight <= 0.001f)
            throw std::runtime_error("XolotlDanza.fbx no tiene altura valida.");
        constexpr float characterScale = 5.0f;
        modelScale = MovementSettings::CharacterHeight * worldScale * 1.12f * characterScale / modelHeight;
        const float height=modelHeight*modelScale;
        lightTarget=position+glm::vec3(0,height*.43f,0);
        lightPosition=lightTarget+glm::vec3(-height*.65f,height*1.25f,height*.8f);
        fillPosition=lightTarget+glm::vec3(height*.65f,height*.9f,-height*.6f);
        lightRange=height*4;
        if (!dancer.HasBakedAnimation())
            throw std::runtime_error("XolotlDanza.xanim no contiene poses para reproducir.");
        std::cout << "Xolotl en el centro del escenario: " << center.x << ", " << center.y << ", "
            << center.z << "; 17 huesos, escala 5.0f (5.6x la nina), giro Y horario de "
            << HeadingDegrees << " grados, baile ciclico activo." << std::endl;
        configured = true;
    }

    void Update(float dt)
    {
        if (!configured) return;
        elapsed += (std::max)(dt, 0.0f);
        // El reloj avanza siempre; la interpolacion/subida GPU se hace solo si se ve.
    }

    bool PrepareVisible(Model& dancer,const glm::mat4& viewProjection) const
    {
        if(!configured||!TransformedBoundsVisible(ExtractFrustum(viewProjection),dancer.GetMinBounds(),dancer.GetMaxBounds(),ModelMatrix(dancer)))return false;
        dancer.UpdateBakedAnimation(elapsed);return true;
    }
    glm::mat4 ModelMatrix(const Model& dancer) const
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, glm::radians(HeadingDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(modelScale));
        return glm::translate(model, glm::vec3(0.0f, -dancer.GetMinBounds().y, 0.0f));
    }

    glm::vec3 ValidationTarget(const Model& dancer) const
    {
        return position + glm::vec3(0.0f, (dancer.GetMaxBounds().y - dancer.GetMinBounds().y) * modelScale * 0.52f, 0.0f);
    }
    glm::vec3 ValidationEye(const Model& dancer) const
    {
        return ValidationTarget(dancer) + glm::vec3(35.0f, 12.0f, 85.0f);
    }

    bool Configured() const { return configured; }
    void UploadLighting(const Shader& shader,const FrustumPlanes* planes=nullptr) const
    {
        shader.Use();const bool visible=configured&&LightSphereVisible(planes,lightPosition,lightRange);
        glUniform1i(shader.Uniform("hasXolotlLight"),visible?1:0);if(!visible)return;
        const auto direction=glm::normalize(lightTarget-lightPosition);
        glUniform3fv(shader.Uniform("xolotlLight.position"),1,&lightPosition[0]);
        glUniform3fv(shader.Uniform("xolotlLight.direction"),1,&direction[0]);
        glUniform3f(shader.Uniform("xolotlLight.color"),2.8f,3.15f,3.8f);
        glUniform1f(shader.Uniform("xolotlLight.range"),lightRange);
        glUniform1f(shader.Uniform("xolotlLight.innerCutoff"),std::cos(glm::radians(26.0f)));
        glUniform1f(shader.Uniform("xolotlLight.outerCutoff"),std::cos(glm::radians(46.0f)));
        glUniform3fv(shader.Uniform("xolotlFill.position"),1,&fillPosition[0]);
        glUniform3f(shader.Uniform("xolotlFill.color"),1.0f,1.2f,1.6f);
        glUniform1f(shader.Uniform("xolotlFill.range"),lightRange*.75f);
    }
    void ValidateLighting(const Shader& shader) const
    {
        UploadLighting(shader);glm::vec3 p(0),d(0);float range=0;
        glGetUniformfv(shader.Program,shader.Uniform("xolotlLight.position"),&p[0]);
        glGetUniformfv(shader.Program,shader.Uniform("xolotlLight.direction"),&d[0]);
        glGetUniformfv(shader.Program,shader.Uniform("xolotlLight.range"),&range);
        if(glm::distance(p,lightPosition)>.001f||glm::dot(d,glm::normalize(lightTarget-p))<.999f||range<=glm::distance(p,lightTarget))
            throw std::runtime_error("La luz de Xolotl no apunta al escenario.");
        std::cout<<"Xolotl: reflector suave dedicado, alineado con su cuerpo y capa."<<std::endl;
    }

private:
    glm::vec3 position = glm::vec3(0.0f);
    float modelScale = 1.0f;
    float elapsed = 0.0f;
    bool configured = false;
    glm::vec3 lightPosition{},lightTarget{},fillPosition{};float lightRange=1;
};
