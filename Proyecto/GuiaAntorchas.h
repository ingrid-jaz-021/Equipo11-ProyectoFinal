#pragma once
#include <algorithm>
#include <array>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "GroundSurface.h"
#include "Shader.h"
#include "GuiaArmPose.h"

// Control de los dos Guias con Antorcha utilizando Modelado Jerarquico Articulado
// (Cinematica Directa) con extremidades independientes (torso, brazo derecho que se balancea
// al costado, brazo izquierdo flexionado al frente con la antorcha vertical) y guiado
// por el pasillo de tapetes de aserrin con giro a la derecha en la esquina Z = -30 hacia el lago.
class GuiaAntorchas
{
public:
    enum class State
    {
        IdleAtEntrance,         // Esperando a la nina en el tapete de la entrada
        GuidingMainCorridor,    // Guiando al frente de la nina por el pasillo principal (-Z)
        TurningRightAtCorner,   // En la esquina Z ≈ -30, girando 90 grados a la derecha hacia +X
        GuidingRightCorridor,   // Guiando al frente de la nina por el pasillo transversal (+X)
        WaitingForGirl,         // Nina desviada o rezagada; esperando en el punto actual
        OpeningAtLake,          // Llegada al lago (X ≈ 0); abriendo paso lateral
        PassedAtLake,           // La nina paso por en medio; guardia lateral junto al lago
        TurningToReturn,        // La nina regresa; girando hacia -X
        ReturningRightCorridor, // Guiando de vuelta hacia la esquina (-X)
        TurningLeftAtCorner,    // En la esquina, girando a la izquierda hacia +Z
        ReturningMainCorridor   // Guiando de regreso a la entrada (+Z)
    };

    void Configure(const Model& hombreCuerpo, const Model& mujerCuerpo,
        const glm::vec3& sourceOrigin, float worldScale)
    {
        origin = sourceOrigin;
        scale = worldScale;

        floorY = 1.776f;
        corridorCenterZ = 242.0f; // Entrada del pasillo
        cornerZ = -29.5f;          // Esquina donde bifurca el tapete hacia la derecha
        mainCorridorX = -114.75f;  // Centro del pasillo principal
        lakeX = -2.0f;             // Final de los tapetes frente al lago

        currentZ = corridorCenterZ;
        currentX = mainCorridorX;

        // Ancho de separacion entre Hombre y Mujer
        hombrePos = glm::vec3(mainCorridorX - 3.25f, floorY, currentZ);
        mujerPos  = glm::vec3(mainCorridorX + 3.25f, floorY, currentZ);

        const glm::vec3 hLo = hombreCuerpo.GetMinBounds();
        const glm::vec3 hHi = hombreCuerpo.GetMaxBounds();
        const glm::vec3 mLo = mujerCuerpo.GetMinBounds();
        const glm::vec3 mHi = mujerCuerpo.GetMaxBounds();

        hombreOrigin = glm::vec3((hLo.x + hHi.x) * 0.5f, hLo.y, (hLo.z + hHi.z) * 0.5f);
        mujerOrigin  = glm::vec3((mLo.x + mHi.x) * 0.5f, mLo.y, (mLo.z + mHi.z) * 0.5f);

        // Proporcion adulta imponente: 5.6m de estatura de cuerpo (pies a cabeza)
        constexpr float bodyHeightInModel = 21.5f;
        constexpr float targetBodyHeight = 5.6f;
        modelScale = targetBodyHeight / bodyHeightInModel; // ≈ 0.260f

        // Source +Z is forward. The torch belongs to the +X arm in BOTH originals.
        // Bind poses and attachment points are generated together in OBJ coordinates.
        hombreRightShoulder = GuiaArmPose::HombreDerShoulder - hombreOrigin;
        hombreLeftShoulder  = GuiaArmPose::HombreIzqShoulder - hombreOrigin;
        mujerRightShoulder  = GuiaArmPose::MujerDerShoulder - mujerOrigin;
        mujerLeftShoulder   = GuiaArmPose::MujerIzqShoulder - mujerOrigin;

        state = State::IdleAtEntrance;
        heading = 180.0f; // Mirando hacia el pasillo / lago (-Z)
        targetHeading = 180.0f;
        lateralOpenFactor = 0.0f;
        walkPhase = 0.0f;
        idlePhase = 0.0f;
        isWalking = false;
        previousStateBeforeWaiting = State::GuidingMainCorridor;
        configured = true;

        std::cout << "GuiaAntorchas (Jerarquico Articulado) OK: Hombros Hombre ("
                  << hombreRightShoulder.x << ", " << hombreRightShoulder.y << ", " << hombreRightShoulder.z
                  << "), escala cuerpo " << targetBodyHeight << "m." << std::endl;
    }

    void Update(float dt, const glm::vec3& playerRuntimePos, const GroundSurface* ground)
    {
        if (!configured || dt <= 0.0f) return;

        const glm::vec3 playerWorld = (scale > 0.0001f) ? (playerRuntimePos / scale + origin) : playerRuntimePos;

        // Comprobacion de si la nina esta sobre el camino de tapetes de aserrin:
        // 1. En el pasillo principal: X cerca de mainCorridorX (-114.75) y Z entre cornerZ y 250
        // 2. En el pasillo lateral derecho: Z cerca de cornerZ (-29.5) y X entre mainCorridorX y lakeX
        const bool onMainCorridor = (std::abs(playerWorld.x - mainCorridorX) <= 6.5f) &&
                                    (playerWorld.z >= cornerZ - 6.0f) && (playerWorld.z <= corridorCenterZ + 6.0f);
        const bool onRightCorridor = (std::abs(playerWorld.z - cornerZ) <= 6.5f) &&
                                     (playerWorld.x >= mainCorridorX - 5.0f) && (playerWorld.x <= lakeX + 6.0f);
        const bool onCarpet = onMainCorridor || onRightCorridor;

        isWalking = false;
        float currentSpeed = 0.0f;

        switch (state)
        {
        case State::IdleAtEntrance:
            targetHeading = 180.0f;
            lateralOpenFactor = 0.0f;
            currentX = mainCorridorX;
            currentZ = corridorCenterZ;
            // Se activa cuando la nina entra al pasillo cerca del tapete de la entrada
            if (onMainCorridor && playerWorld.z <= 246.0f && playerWorld.z >= 228.0f)
            {
                state = State::GuidingMainCorridor;
            }
            break;

        case State::GuidingMainCorridor:
            targetHeading = 180.0f;
            currentX = mainCorridorX;

            // Si la nina se desvia o se queda atras a mas de 12m en Z
            if (!onCarpet || (playerWorld.z - currentZ) > 12.0f)
            {
                previousStateBeforeWaiting = State::GuidingMainCorridor;
                state = State::WaitingForGirl;
                break;
            }

            // Los guias avanzan al frente de la nina (~5.5m adelante en -Z)
            {
                const float desiredZ = glm::clamp(playerWorld.z - 5.5f, cornerZ, corridorCenterZ);
                if (currentZ > desiredZ + 0.1f)
                {
                    isWalking = true;
                    currentSpeed = glm::clamp((currentZ - desiredZ) * 2.5f, 1.2f, 5.5f);
                    currentZ -= currentSpeed * dt;
                    if (currentZ < desiredZ) currentZ = desiredZ;
                }
            }

            // Llegada a la esquina Z ≈ -29.5f
            if (currentZ <= cornerZ + 0.5f)
            {
                currentZ = cornerZ;
                state = State::TurningRightAtCorner;
            }
            break;

        case State::TurningRightAtCorner:
            // Girar 90 grados a la derecha: de heading 180° a heading 90° (mirando hacia +X)
            targetHeading = 90.0f;
            currentX = mainCorridorX;
            currentZ = cornerZ;
            if (std::abs(heading - targetHeading) < 6.0f)
            {
                heading = 90.0f;
                state = State::GuidingRightCorridor;
            }
            break;

        case State::GuidingRightCorridor:
            targetHeading = 90.0f;
            currentZ = cornerZ;

            // Si la nina se desvia o se retrasa a mas de 12m en X
            if (!onCarpet || (currentX - playerWorld.x) > 12.0f)
            {
                previousStateBeforeWaiting = State::GuidingRightCorridor;
                state = State::WaitingForGirl;
                break;
            }

            // Los guias avanzan al frente de la nina (~5.5m adelante en +X)
            {
                const float desiredX = glm::clamp(playerWorld.x + 5.5f, mainCorridorX, lakeX);
                if (currentX < desiredX - 0.1f)
                {
                    isWalking = true;
                    currentSpeed = glm::clamp((desiredX - currentX) * 2.5f, 1.2f, 5.5f);
                    currentX += currentSpeed * dt;
                    if (currentX > desiredX) currentX = desiredX;
                }
            }

            // Llegada al final del camino junto al lago (X ≈ -2.0f)
            if (currentX >= lakeX - 1.0f)
            {
                currentX = lakeX;
                state = State::OpeningAtLake;
            }
            break;

        case State::WaitingForGirl:
            // Esperan en su posicion exacta hasta que la nina vuelva al camino
            if (previousStateBeforeWaiting == State::GuidingMainCorridor)
            {
                targetHeading = 180.0f;
                // Si la nina regresa cerca en el pasillo
                if (onMainCorridor && (playerWorld.z - currentZ) <= 8.5f && (playerWorld.z - currentZ) >= -3.0f)
                {
                    state = State::GuidingMainCorridor;
                }
                // Si la nina retrocede mucho queriendo volver a la entrada
                else if (onMainCorridor && playerWorld.z > currentZ + 18.0f)
                {
                    state = State::ReturningMainCorridor;
                }
            }
            else // previousStateBeforeWaiting == State::GuidingRightCorridor
            {
                targetHeading = 90.0f;
                if (onRightCorridor && (currentX - playerWorld.x) <= 8.5f && (currentX - playerWorld.x) >= -3.0f)
                {
                    state = State::GuidingRightCorridor;
                }
                else if (onRightCorridor && playerWorld.x < currentX - 16.0f)
                {
                    state = State::TurningToReturn;
                }
            }
            break;

        case State::OpeningAtLake:
            // Se abren lateralmente (separandose en Z) para dejar pasar a la nina
            targetHeading = 90.0f;
            currentX = lakeX;
            currentZ = cornerZ;
            lateralOpenFactor = glm::min(lateralOpenFactor + dt * 2.0f, 1.0f);
            if (playerWorld.x > currentX + 1.0f)
            {
                state = State::PassedAtLake;
            }
            break;

        case State::PassedAtLake:
            targetHeading = 90.0f;
            lateralOpenFactor = 1.0f;
            // Cuando la nina regrese desde el lago hacia el pasillo (X < lakeX - 1.0f)
            if (playerWorld.x < lakeX - 1.5f && onRightCorridor)
            {
                state = State::TurningToReturn;
            }
            break;

        case State::TurningToReturn:
            // Giro de 180 grados: mirando hacia -X (heading 270°)
            targetHeading = 270.0f;
            lateralOpenFactor = glm::max(lateralOpenFactor - dt * 2.0f, 0.0f);
            if (std::abs(heading - targetHeading) < 6.0f && lateralOpenFactor <= 0.05f)
            {
                heading = 270.0f;
                state = State::ReturningRightCorridor;
            }
            break;

        case State::ReturningRightCorridor:
            targetHeading = 270.0f;
            currentZ = cornerZ;
            if (!onCarpet || (playerWorld.x - currentX) > 12.0f)
            {
                // Espera en el sitio
                break;
            }

            // Avanzan al frente de la nina en -X hacia la esquina
            {
                const float desiredX = glm::clamp(playerWorld.x - 5.5f, mainCorridorX, lakeX);
                if (currentX > desiredX + 0.1f)
                {
                    isWalking = true;
                    currentSpeed = glm::clamp((currentX - desiredX) * 2.5f, 1.2f, 5.5f);
                    currentX -= currentSpeed * dt;
                    if (currentX < desiredX) currentX = desiredX;
                }
            }

            // Llegada a la esquina
            if (currentX <= mainCorridorX + 0.5f)
            {
                currentX = mainCorridorX;
                state = State::TurningLeftAtCorner;
            }
            break;

        case State::TurningLeftAtCorner:
            // Giro a la izquierda: de heading 270° a heading 0° (mirando hacia +Z, la entrada)
            targetHeading = 0.0f;
            currentX = mainCorridorX;
            currentZ = cornerZ;
            if (std::abs(heading - targetHeading) < 6.0f)
            {
                heading = 0.0f;
                state = State::ReturningMainCorridor;
            }
            break;

        case State::ReturningMainCorridor:
            targetHeading = 0.0f;
            currentX = mainCorridorX;
            if (!onCarpet || (currentZ - playerWorld.z) > 12.0f)
            {
                break;
            }

            // Avanzan al frente de la nina en +Z hacia la entrada
            {
                const float desiredZ = glm::clamp(playerWorld.z + 5.5f, cornerZ, corridorCenterZ);
                if (currentZ < desiredZ - 0.1f)
                {
                    isWalking = true;
                    currentSpeed = glm::clamp((desiredZ - currentZ) * 2.5f, 1.2f, 5.5f);
                    currentZ += currentSpeed * dt;
                    if (currentZ > desiredZ) currentZ = desiredZ;
                }
            }

            // Llegada a la entrada original
            if (currentZ >= corridorCenterZ - 0.5f)
            {
                currentZ = corridorCenterZ;
                state = State::IdleAtEntrance;
                targetHeading = 180.0f;
            }
            break;
        }

        // Interpolacion suave de orientacion (giros angulares)
        {
            float diff = targetHeading - heading;
            while (diff > 180.0f) diff -= 360.0f;
            while (diff < -180.0f) diff += 360.0f;
            heading += glm::clamp(diff, -200.0f * dt, 200.0f * dt);
            while (heading < 0.0f) heading += 360.0f;
            while (heading >= 360.0f) heading -= 360.0f;
        }

        // Calculo de posiciones Hombre y Mujer en funcion del tramo y apertura lateral:
        // En pasillo principal (alineado en Z): Hombre a X - 3.25, Mujer a X + 3.25
        // En pasillo lateral (alineado en X): Hombre a Z - 3.25, Mujer a Z + 3.25
        // Apertura en el lago: amplifica la separacion
        if (state == State::GuidingMainCorridor || state == State::IdleAtEntrance || state == State::ReturningMainCorridor)
        {
            hombrePos.x = currentX - 3.25f;
            hombrePos.z = currentZ;
            mujerPos.x  = currentX + 3.25f;
            mujerPos.z  = currentZ;
        }
        else if (state == State::GuidingRightCorridor || state == State::ReturningRightCorridor ||
                 state == State::OpeningAtLake || state == State::PassedAtLake || state == State::TurningToReturn)
        {
            const float baseSep = 3.25f + lateralOpenFactor * 4.5f; // se abren hasta 7.75m a cada lado
            hombrePos.x = currentX;
            hombrePos.z = currentZ - baseSep;
            mujerPos.x  = currentX;
            mujerPos.z  = currentZ + baseSep;
        }
        else // Transiciones de giro en la esquina
        {
            const float tGiro = glm::clamp(std::abs(heading - 180.0f) / 90.0f, 0.0f, 1.0f);
            hombrePos.x = glm::mix(currentX - 3.25f, currentX, tGiro);
            hombrePos.z = glm::mix(currentZ, currentZ - 3.25f, tGiro);
            mujerPos.x  = glm::mix(currentX + 3.25f, currentX, tGiro);
            mujerPos.z  = glm::mix(currentZ, currentZ + 3.25f, tGiro);
        }

        // Muestreo de altura del piso para contacto exacto con el tapete
        if (ground)
        {
            float yH = floorY;
            if (ground->SampleCharacter(hombrePos.x, hombrePos.z, origin.y, yH))
                hombrePos.y = yH;
            float yM = floorY;
            if (ground->SampleCharacter(mujerPos.x, mujerPos.z, origin.y, yM))
                mujerPos.y = yM;
        }

        // === MODELADO JERARQUICO ARTICULADO (Cinematica Directa) ===
        if (isWalking)
        {
            constexpr float walkFrequency = 2.0f; // 2 pasos por segundo
            walkPhase += walkFrequency * 6.2831853f * dt;
            if (walkPhase > 628.31853f) walkPhase -= 628.31853f;

            // Torso: leve rebote vertical y balanceo lateral (roll)
            torsoBobY = 0.12f * std::abs(std::sin(walkPhase));
            torsoRoll = 1.8f * std::sin(walkPhase);
            torsoPitch = 2.0f;

            // Extremidad articulada: Brazo derecho colgado al costado con balanceo pendular real
            rightArmSwing = 25.0f * std::sin(walkPhase);

            // Codo izquierdo flexionado en bind pose; leve oscilacion al llevar la antorcha.
            torchBobPitch = 1.8f * std::cos(walkPhase * 0.5f);
        }
        else
        {
            constexpr float idleFrequency = 0.8f;
            idlePhase += idleFrequency * 6.2831853f * dt;
            if (idlePhase > 628.31853f) idlePhase -= 628.31853f;

            torsoBobY = 0.03f * std::sin(idlePhase);
            torsoRoll = 0.5f * std::cos(idlePhase * 0.7f);
            torsoPitch = 0.0f;
            rightArmSwing = 0.0f;
            torchBobPitch = 0.8f * std::sin(idlePhase * 1.2f);
        }
    }

    // --- MATRICES JERARQUICAS: HOMBRE ---
    glm::mat4 HombreCuerpoMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (hombrePos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(heading), glm::vec3(0.0f, 1.0f, 0.0f));

        // Nivel 1: Torso / Cuerpo
        m = glm::translate(m, glm::vec3(0.0f, torsoBobY, 0.0f));
        m = glm::rotate(m, glm::radians(torsoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(-torsoRoll), glm::vec3(0.0f, 0.0f, 1.0f));

        m = glm::translate(m, -hombreOrigin);
        return m;
    }

    glm::mat4 HombreBrazoDerMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (hombrePos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(heading), glm::vec3(0.0f, 1.0f, 0.0f));

        // Torso
        m = glm::translate(m, glm::vec3(0.0f, torsoBobY, 0.0f));
        m = glm::rotate(m, glm::radians(torsoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(-torsoRoll), glm::vec3(0.0f, 0.0f, 1.0f));

        // Nivel 2: Articulacion del hombro derecho (balanceo pendular adelante/atras)
        m = glm::translate(m, hombreRightShoulder);
        m = glm::rotate(m, glm::radians(rightArmSwing), glm::vec3(1.0f, 0.0f, 0.0f));
        return m;
    }

    glm::mat4 HombreBrazoIzqMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (hombrePos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(heading), glm::vec3(0.0f, 1.0f, 0.0f));

        // Torso
        m = glm::translate(m, glm::vec3(0.0f, torsoBobY, 0.0f));
        m = glm::rotate(m, glm::radians(torsoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(-torsoRoll), glm::vec3(0.0f, 0.0f, 1.0f));

        // Nivel 2: hombro izquierdo; el codo flexionado y la mano apuntan hacia +Z.
        m = glm::translate(m, hombreLeftShoulder);
        m = glm::rotate(m, glm::radians(torchBobPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        return m;
    }

    // --- MATRICES JERARQUICAS: MUJER ---
    glm::mat4 MujerCuerpoMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (mujerPos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(heading), glm::vec3(0.0f, 1.0f, 0.0f));

        m = glm::translate(m, glm::vec3(0.0f, torsoBobY, 0.0f));
        m = glm::rotate(m, glm::radians(torsoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(torsoRoll), glm::vec3(0.0f, 0.0f, 1.0f));

        m = glm::translate(m, -mujerOrigin);
        return m;
    }

    glm::mat4 MujerBrazoDerMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (mujerPos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(heading), glm::vec3(0.0f, 1.0f, 0.0f));

        m = glm::translate(m, glm::vec3(0.0f, torsoBobY, 0.0f));
        m = glm::rotate(m, glm::radians(torsoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(torsoRoll), glm::vec3(0.0f, 0.0f, 1.0f));

        m = glm::translate(m, mujerRightShoulder);
        m = glm::rotate(m, glm::radians(-rightArmSwing), glm::vec3(1.0f, 0.0f, 0.0f));
        return m;
    }

    glm::mat4 MujerBrazoIzqMatrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), (mujerPos - origin) * scale);
        m = glm::scale(m, glm::vec3(scale * modelScale));
        m = glm::rotate(m, glm::radians(heading), glm::vec3(0.0f, 1.0f, 0.0f));

        m = glm::translate(m, glm::vec3(0.0f, torsoBobY, 0.0f));
        m = glm::rotate(m, glm::radians(torsoPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(torsoRoll), glm::vec3(0.0f, 0.0f, 1.0f));

        m = glm::translate(m, mujerLeftShoulder);
        m = glm::rotate(m, glm::radians(torchBobPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        return m;
    }

    // Posiciones en el mundo de las llamas de las antorchas para fuentes de luz puntual
    glm::vec3 TorchLightPositionHombre() const
    {
        const glm::vec4 p = HombreBrazoIzqMatrix() * glm::vec4(GuiaArmPose::HombreTorch, 1.0f);
        return glm::vec3(p) / (p.w != 0.0f ? p.w : 1.0f);
    }

    glm::vec3 TorchLightPositionMujer() const
    {
        const glm::vec4 p = MujerBrazoIzqMatrix() * glm::vec4(GuiaArmPose::MujerTorch, 1.0f);
        return glm::vec3(p) / (p.w != 0.0f ? p.w : 1.0f);
    }

    struct TorchPointLight
    {
        glm::vec3 position, diffuse, specular;
        float constant, linear, quadratic, range;
    };
    using TorchFrame = std::array<TorchPointLight,2>;

    // Build once AFTER Update; reuse this same frame for static, skinned and
    // instanced geometry. The socket is the measured center of each flame mesh.
    TorchFrame TorchLightsAt(double now) const
    {
        TorchFrame lights;
        const glm::vec3 positions[]={TorchLightPositionHombre(),TorchLightPositionMujer()};
        const float worldUnit=std::max(scale,.0001f);
        for(size_t i=0;i<lights.size();++i)
        {
            const double t=now*(1.0+i*.073),phase=i*2.39;
            const float flicker=1.0f+static_cast<float>(.08*std::sin(t*7.1+phase)+
                .045*std::sin(t*13.7+phase*1.7)+.025*std::sin(t*23.9+phase*.7));
            lights[i]={positions[i],glm::vec3(1,.48f,.105f)*(2.65f*flicker),
                glm::vec3(1,.64f,.27f)*(.38f*flicker),
                1.0f,.14f/worldUnit,.07f/(worldUnit*worldUnit),14.0f*worldUnit};
        }
        return lights;
    }

    void UploadTorchLights(const Shader& shader,const TorchFrame& lights,const FrustumPlanes* planes=nullptr) const
    {
        shader.Use();int visible=0;
        if(configured)for(const auto& light:lights)
        {
            // Retain off-screen flames when their light still reaches visible surfaces.
            if(!LightSphereVisible(planes,light.position,light.range))continue;
            const auto prefix="torchLights["+std::to_string(visible++)+"].";
            glUniform3fv(shader.Uniform(prefix+"position"),1,&light.position[0]);
            glUniform3fv(shader.Uniform(prefix+"diffuse"),1,&light.diffuse[0]);
            glUniform3fv(shader.Uniform(prefix+"specular"),1,&light.specular[0]);
            glUniform1f(shader.Uniform(prefix+"constant"),light.constant);
            glUniform1f(shader.Uniform(prefix+"linear"),light.linear);
            glUniform1f(shader.Uniform(prefix+"quadratic"),light.quadratic);
            glUniform1f(shader.Uniform(prefix+"range"),light.range);
        }
        glUniform1i(shader.Uniform("torchLightCount"),visible);
    }

    void ValidateTorchLighting(const Shader& environment,const Shader& character,const Shader& instanced) const
    {
        GuiaAntorchas moving=*this;
        const auto start=moving.TorchLightsAt(0);
        const auto player=(glm::vec3(mainCorridorX,floorY,239)-origin)*scale;
        for(int i=0;i<120;++i)moving.Update(1.0f/60,player,nullptr);
        if(glm::distance(moving.TorchLightsAt(2)[0].position,start[0].position)<scale)
            throw std::runtime_error("La luz de la antorcha no sigue al NPC.");
        for(const Shader* shader:{&environment,&character,&instanced})
        {
            const glm::vec3 viewer(31,47,59);shader->Use();
            glUniform3fv(shader->Uniform("viewPosition"),1,&viewer[0]);
            glm::vec3 uploadedViewer(0);
            glGetUniformfv(shader->Program,shader->Uniform("viewPosition"),&uploadedViewer[0]);
            if(glm::distance(viewer,uploadedViewer)>.001f)throw std::runtime_error("Falta la camara para los reflejos del fuego.");
            for(int direction=0;direction<4;++direction)
            {
                moving.heading=direction*90.0f;
                const auto lights=moving.TorchLightsAt(2.0+direction*.37);
                moving.UploadTorchLights(*shader,lights);
                GLint count=0;glGetUniformiv(shader->Program,shader->Uniform("torchLightCount"),&count);
                if(count!=2)throw std::runtime_error("Falta una antorcha en el shader.");
                for(size_t i=0;i<lights.size();++i)
                {
                    const auto& light=lights[i];const auto prefix="torchLights["+std::to_string(i)+"].";
                    auto vector=[&](const char* name,const glm::vec3& expected)
                    {
                        glm::vec3 value(0);const auto location=shader->Uniform(prefix+name);
                        if(location<0)throw std::runtime_error("Uniform de antorcha ausente: "+prefix+name);
                        glGetUniformfv(shader->Program,location,&value[0]);
                        if(glm::distance(value,expected)>.001f)throw std::runtime_error("Uniform de antorcha desactualizado: "+prefix+name);
                    };
                    auto scalar=[&](const char* name,float expected)
                    {
                        float value=0;const auto location=shader->Uniform(prefix+name);
                        if(location<0)throw std::runtime_error("Uniform de atenuacion ausente: "+prefix+name);
                        glGetUniformfv(shader->Program,location,&value);
                        if(std::abs(value-expected)>.00001f)throw std::runtime_error("Atenuacion de antorcha incorrecta.");
                    };
                    vector("position",light.position);vector("diffuse",light.diffuse);vector("specular",light.specular);
                    scalar("constant",light.constant);scalar("linear",light.linear);
                    scalar("quadratic",light.quadratic);scalar("range",light.range);
                    const float floor=(floorY-origin.y)*scale;
                    if(light.range<2*(light.position.y-floor))throw std::runtime_error("La antorcha no alcanza el piso.");
                }
            }
            UploadTorchLights(*shader,TorchLightsAt(0));
        }
        const auto a=TorchLightsAt(0),b=TorchLightsAt(.19);
        if(glm::distance(a[0].diffuse,b[0].diffuse)<.01f||glm::distance(a[1].diffuse,b[1].diffuse)<.01f)
            throw std::runtime_error("Falta el parpadeo independiente de las antorchas.");
        std::cout<<"Antorchas GPU OK: 2 luces moviles, 4 orientaciones, atenuacion y reflejos en 3 shaders; alcance hasta el piso."<<std::endl;
    }

    bool Configured() const { return configured; }

    // Validation uses real model bounds and rendered attachment matrices. In particular
    // it catches an importer-axis error or swapped shoulders even if OBJ loading succeeds.
    void ValidateAnatomy(const Model& hombreArm, const Model& mujerArm) const
    {
        for (const Model* arm : { &hombreArm, &mujerArm })
        {
            const auto lo = arm->GetMinBounds(), hi = arm->GetMaxBounds();
            if (lo.z < -1.8f || hi.z > 4.5f || hi.x-lo.x > 3.5f)
                throw std::runtime_error("Guia: brazo fuera de su espacio anatomico; revisar ejes OBJ.");
        }
        auto check = [](const glm::mat4& body, const glm::mat4& arm,
            const glm::vec3& shoulder, const glm::vec3& elbow, const glm::vec3& wrist)
        {
            const auto seam = body * glm::vec4(shoulder, 1);
            if (glm::length(glm::vec3(seam-arm*glm::vec4(0,0,0,1))) > .001f)
                throw std::runtime_error("Guia: brazo separado del hombro.");
            const auto local = glm::inverse(body)*arm;
            const auto e = glm::vec3(local*glm::vec4(elbow,1))-shoulder;
            const auto w = glm::vec3(local*glm::vec4(wrist,1))-shoulder;
            const float angle = glm::degrees(std::acos(glm::clamp(glm::dot(glm::normalize(e),glm::normalize(w-e)),-1.0f,1.0f)));
            if (e.y > -1 || w.y > -.8f || w.z < 1.5f || std::abs(w.x) > .9f || angle < 75 || angle > 95)
                throw std::runtime_error("Guia: codo recto o antorcha orientada hacia un lado.");
        };
        GuiaAntorchas probe = *this;
        for (int direction=0; direction<4; ++direction)
            for (int phase=0; phase<24; ++phase)
            {
                probe.heading=direction*90.0f;
                probe.torsoPitch=2;probe.torsoRoll=1.8f*std::sin(phase*.3f);
                probe.torchBobPitch=1.8f*std::cos(phase*.3f);
                check(probe.HombreCuerpoMatrix(),probe.HombreBrazoIzqMatrix(),
                    GuiaArmPose::HombreIzqShoulder,GuiaArmPose::HombreIzqElbow,GuiaArmPose::HombreIzqWrist);
                check(probe.MujerCuerpoMatrix(),probe.MujerBrazoIzqMatrix(),
                    GuiaArmPose::MujerIzqShoulder,GuiaArmPose::MujerIzqElbow,GuiaArmPose::MujerIzqWrist);
            }
        std::cout << "Guias OK: codos flexionados, manos al frente y hombros unidos en 4 orientaciones." << std::endl;
    }

private:
    glm::vec3 hombrePos = glm::vec3(0.0f);
    glm::vec3 mujerPos = glm::vec3(0.0f);
    glm::vec3 hombreOrigin = glm::vec3(0.0f);
    glm::vec3 mujerOrigin = glm::vec3(0.0f);
    glm::vec3 hombreRightShoulder = glm::vec3(0.0f);
    glm::vec3 hombreLeftShoulder = glm::vec3(0.0f);
    glm::vec3 mujerRightShoulder = glm::vec3(0.0f);
    glm::vec3 mujerLeftShoulder = glm::vec3(0.0f);

    glm::vec3 origin = glm::vec3(0.0f);
    float scale = 1.0f;
    float modelScale = 0.260f;
    float floorY = 1.776f;
    float corridorCenterZ = 242.0f;
    float cornerZ = -29.5f;
    float mainCorridorX = -114.75f;
    float lakeX = -2.0f;

    float currentZ = 242.0f;
    float currentX = -114.75f;

    State state = State::IdleAtEntrance;
    State previousStateBeforeWaiting = State::GuidingMainCorridor;
    float heading = 180.0f;
    float targetHeading = 180.0f;
    float lateralOpenFactor = 0.0f;

    // Variables de cinemática directa (articulaciones)
    float torsoBobY = 0.0f;
    float torsoPitch = 0.0f;
    float torsoRoll = 0.0f;
    float rightArmSwing = 0.0f;
    float torchBobPitch = 0.0f;

    float walkPhase = 0.0f;
    float idlePhase = 0.0f;
    bool isWalking = false;
    bool configured = false;
};
