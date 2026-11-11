#pragma once
#include <algorithm>
#include <iostream>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Model.h"
#include "GroundSurface.h"
#include "Shader.h"

// Control de los dos Guias con Antorcha utilizando Modelado Jerarquico Articulado
// (Cinematica Directa) con extremidades independientes (torso, brazo derecho que se balancea
// al costado, brazo izquierdo estirado al frente con la antorcha vertical) y guiado
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

    void Configure(const Model& environment, const Model& hombreCuerpo, const Model& mujerCuerpo,
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

        // Articulaciones en la boca de la manga (el hueso emerge limpio sin vestimenta duplicada)
        // HombreGA: manga derecha (-16.20, 12.60, -5.20), manga izquierda (-22.45, 12.60, -5.20)
        hombreRightShoulder = glm::vec3(-16.20f, 12.60f, -5.20f) - hombreOrigin;
        hombreLeftShoulder  = glm::vec3(-22.45f, 12.60f, -5.20f) - hombreOrigin;

        // MujerGA: manga derecha (14.65, 13.50, -3.40), manga izquierda (8.40, 13.50, -3.40)
        mujerRightShoulder  = glm::vec3(14.65f, 13.50f, -3.40f) - mujerOrigin;
        mujerLeftShoulder   = glm::vec3(8.40f,  13.50f, -3.40f) - mujerOrigin;

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
                std::cout << "GuiaAntorchas: Nina detectada en la entrada. Guiando por el pasillo principal." << std::endl;
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
                std::cout << "GuiaAntorchas: Nina desviada o retrasada en pasillo principal. Esperando en Z = " << currentZ << std::endl;
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
                std::cout << "GuiaAntorchas: Llegada a la esquina. Girando a la derecha hacia el camino de tapetes." << std::endl;
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
                std::cout << "GuiaAntorchas: Giro completado. Guiando por el camino lateral hacia el lago." << std::endl;
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
                std::cout << "GuiaAntorchas: Nina desviada o retrasada en camino lateral. Esperando en X = " << currentX << std::endl;
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
                std::cout << "GuiaAntorchas: Llegada al lago. Abriendo paso lateral para la nina." << std::endl;
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
                    std::cout << "GuiaAntorchas: Nina regreso al pasillo principal. Reanudando marcha." << std::endl;
                }
                // Si la nina retrocede mucho queriendo volver a la entrada
                else if (onMainCorridor && playerWorld.z > currentZ + 18.0f)
                {
                    state = State::ReturningMainCorridor;
                    std::cout << "GuiaAntorchas: Nina retrocede hacia la entrada. Regresando." << std::endl;
                }
            }
            else // previousStateBeforeWaiting == State::GuidingRightCorridor
            {
                targetHeading = 90.0f;
                if (onRightCorridor && (currentX - playerWorld.x) <= 8.5f && (currentX - playerWorld.x) >= -3.0f)
                {
                    state = State::GuidingRightCorridor;
                    std::cout << "GuiaAntorchas: Nina regreso al camino lateral. Reanudando marcha hacia el lago." << std::endl;
                }
                else if (onRightCorridor && playerWorld.x < currentX - 16.0f)
                {
                    state = State::TurningToReturn;
                    std::cout << "GuiaAntorchas: Nina retrocede hacia la esquina. Iniciando retorno." << std::endl;
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
                std::cout << "GuiaAntorchas: Nina paso por en medio hacia el lago. Guias en guardia lateral." << std::endl;
            }
            break;

        case State::PassedAtLake:
            targetHeading = 90.0f;
            lateralOpenFactor = 1.0f;
            // Cuando la nina regrese desde el lago hacia el pasillo (X < lakeX - 1.0f)
            if (playerWorld.x < lakeX - 1.5f && onRightCorridor)
            {
                state = State::TurningToReturn;
                std::cout << "GuiaAntorchas: Nina regresa del lago. Guias volteandose para guiar retorno." << std::endl;
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
                std::cout << "GuiaAntorchas: Giro completado. Guiando de regreso hacia la esquina." << std::endl;
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
                std::cout << "GuiaAntorchas: Llegada a la esquina en retorno. Girando a la izquierda hacia la entrada." << std::endl;
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
                std::cout << "GuiaAntorchas: Giro completado. Guiando de regreso por el pasillo principal hacia la entrada." << std::endl;
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
                std::cout << "GuiaAntorchas: Regreso completado a la entrada del pasillo." << std::endl;
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

            // Brazo izquierdo: sostenido estirado al frente con oscilacion ceremonial muy suave
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

        // Nivel 2: Articulacion del hombro izquierdo (estirado al frente con antorcha)
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
        const glm::vec4 p = HombreBrazoIzqMatrix() * glm::vec4(-0.59f, 3.26f, -1.99f, 1.0f);
        return glm::vec3(p) / (p.w != 0.0f ? p.w : 1.0f);
    }

    glm::vec3 TorchLightPositionMujer() const
    {
        const glm::vec4 p = MujerBrazoIzqMatrix() * glm::vec4(-0.19f, 3.35f, -3.21f, 1.0f);
        return glm::vec3(p) / (p.w != 0.0f ? p.w : 1.0f);
    }

    // Actualiza las luces puntuales dinamicas (point lights) calidas/fuego con parpadeo organico
    void UploadTorchLights(const Shader& shader, float now) const
    {
        if (!configured) return;
        shader.Use();

        // Parpadeo organico del fuego mediante combinacion de frecuencias
        const float flickerH = 0.12f * std::sin(now * 15.3f) + 0.07f * std::sin(now * 27.7f) + 0.04f * std::cos(now * 41.1f);
        const float flickerM = 0.12f * std::sin(now * 17.1f + 1.5f) + 0.07f * std::sin(now * 31.3f) + 0.04f * std::cos(now * 43.7f);

        // Color fuego calido (luz ambar/dorada)
        const glm::vec3 baseFireColor(1.0f, 0.58f, 0.16f);
        const glm::vec3 colorHombre = baseFireColor * (1.65f + flickerH);
        const glm::vec3 colorMujer  = baseFireColor * (1.65f + flickerM);

        constexpr float range = 24.0f; // Alcance para iluminar el suelo de aserrin y los personajes

        const glm::vec3 posHombre = TorchLightPositionHombre();
        const glm::vec3 posMujer  = TorchLightPositionMujer();

        glUniform1i(shader.Uniform("torchLightCount"), 2);

        glUniform3fv(shader.Uniform("torchLights[0].position"), 1, &posHombre[0]);
        glUniform3fv(shader.Uniform("torchLights[0].color"), 1, &colorHombre[0]);
        glUniform1f(shader.Uniform("torchLights[0].range"), range);

        glUniform3fv(shader.Uniform("torchLights[1].position"), 1, &posMujer[0]);
        glUniform3fv(shader.Uniform("torchLights[1].color"), 1, &colorMujer[0]);
        glUniform1f(shader.Uniform("torchLights[1].range"), range);
    }

    bool Configured() const { return configured; }
    State CurrentState() const { return state; }
    float CurrentZ() const { return currentZ; }
    float CurrentX() const { return currentX; }
    bool IsWalking() const { return isWalking; }

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
