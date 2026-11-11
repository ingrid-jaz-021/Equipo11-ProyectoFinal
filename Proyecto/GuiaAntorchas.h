#pragma once
#include <algorithm>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"

class GuiaAntorchas
{
public:
    void Configure(const Model& environment, const Model& hombre, const Model& mujer,
        const glm::vec3& sourceOrigin, float worldScale)
    {
        // En el Pasillo real, el tapete de aserrin con la calavera (TapeteAserrin1.002)
        // se extiende frente al arco de cempasuchil de la entrada (Z ≈ 246.8).
        // Los dos guias se situan sobre el tapete mirando hacia el interior del pasillo (-Z),
        // con el Hombre a la izquierda sobre la greca y la Mujer al centro-derecha
        // sobre el penacho superior de la calavera, tal como en Escenario_2.blend.
        constexpr float floorY = 1.776f;
        const glm::vec3 hombreCoord(-118.2f, floorY, 242.5f);
        const glm::vec3 mujerCoord(-112.2f, floorY, 241.5f);

        const glm::vec3 hLo = hombre.GetMinBounds();
        const glm::vec3 hHi = hombre.GetMaxBounds();
        const glm::vec3 mLo = mujer.GetMinBounds();
        const glm::vec3 mHi = mujer.GetMaxBounds();

        hombreOrigin = glm::vec3((hLo.x + hHi.x) * 0.5f, hLo.y, (hLo.z + hHi.z) * 0.5f);
        mujerOrigin = glm::vec3((mLo.x + mHi.x) * 0.5f, mLo.y, (mLo.z + mHi.z) * 0.5f);

        // Estatura del cuerpo humano del modelo (pies a coronilla, sin contar penacho):
        // En HombreGA: pies Y=1.01, cabeza Y≈22.5 (altura de cuerpo ≈ 21.5 unidades).
        // Frente a la nina del juego (~2.8m), una estatura de cuerpo adulto de ~5.6m
        // da una proporcion visual imponente de adulto a nina (~2.0x), elevando la altura total
        // con penacho ceremonial a ~7.8m.
        constexpr float bodyHeightInModel = 21.5f;
        constexpr float targetBodyHeight = 5.6f;
        modelScale = targetBodyHeight / bodyHeightInModel;

        hombrePos = hombreCoord;
        mujerPos = mujerCoord;

        origin = sourceOrigin;
        scale = worldScale;
        configured = true;

        std::cout << "Guias con antorcha OK: Hombre en (" << hombrePos.x << ", " << hombrePos.y << ", " << hombrePos.z
                  << "), Mujer en (" << mujerPos.x << ", " << mujerPos.y << ", " << mujerPos.z
                  << "), escala " << modelScale << " (cuerpo adulto " << targetBodyHeight
                  << "m sin penacho, altura total con penacho " << (hHi.y - hLo.y) * modelScale << "m)." << std::endl;
    }

    glm::mat4 HombreMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (hombrePos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::translate(m, -hombreOrigin);
        return m;
    }

    glm::mat4 MujerMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (mujerPos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::translate(m, -mujerOrigin);
        return m;
    }

    bool Configured() const { return configured; }

private:
    glm::vec3 hombrePos = glm::vec3(0.0f);
    glm::vec3 mujerPos = glm::vec3(0.0f);
    glm::vec3 hombreOrigin = glm::vec3(0.0f);
    glm::vec3 mujerOrigin = glm::vec3(0.0f);
    glm::vec3 origin = glm::vec3(0.0f);
    float scale = 1.0f;
    float modelScale = 0.223f;
    bool configured = false;
};
