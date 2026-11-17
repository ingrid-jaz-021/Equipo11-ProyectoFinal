#pragma once
#include <cmath>
#include "CharacterController.h"
#include "Model.h"
#include "NpcDialogue.h"

class AxoloteNpc:public CharacterController
{
public:
    enum class MotionPhase{Idle,Raising,Greeting,Returning};
    MotionPhase CurrentMotionPhase() const
    {return clock<1.4||clock>=6.4?MotionPhase::Idle:clock<2.6?MotionPhase::Raising:clock<5.2?MotionPhase::Greeting:MotionPhase::Returning;}
    AxoloteNpc():dialogue({"images/AxoloteDialogo0.png","images/AxoloteDialogo1.png","images/AxoloteDialogo2.png","images/AxoloteDialogo3.png"}){}
    void ConfigureNpc(Model& model,const Model* environment,const GroundSurface& ground,const glm::vec3& origin,float scale)
    {
        worldScale=scale;rig=model.Animator();Attach(rig);SetFrozen(true);
        Configure(model.GetMinBounds(),model.GetMaxBounds(),4.4f*scale);
        glm::vec3 signMin(-126.272f,1.657282f,247.8004f),signMax(-120.3086f,6.369077f,248.0148f);
        if(environment&&!environment->GetMaterialBounds("letrero_bienvenida.001",signMin,signMax))throw std::runtime_error("Falta el letrero azul del festival.");
        const float halfWidth=(model.GetMaxBounds().x-model.GetMinBounds().x)*modelScale/scale*.5f;
        glm::vec3 source(signMax.x+.65f+halfWidth,signMin.y,(signMin.z+signMax.z)*.5f);
        const float signZ=source.z;float support=source.y;bool grounded=!environment;
        // Buscar el apoyo mas cercano al propio letrero, dentro de 1.5 unidades.
        for(int step=0;environment&&!grounded&&step<=12;++step)
        {
            const float dz=step==0?0:(step%2?1:-1)*((step+1)/2)*.25f;
            if(ground.Sample(source.x,signZ+dz,signMin.y,support)){source.z=signZ+dz;grounded=true;}
        }
        if(!grounded)throw std::runtime_error("No hay piso junto al letrero azul.");
        source.y=support;position=(source-origin)*scale;yaw=0;
        signCenter=((signMin+signMax)*.5f-origin)*scale;
        if(std::abs(source.z-signZ)>1.51f||source.x-halfWidth<signMax.x+.6f)throw std::runtime_error("Axolote desalineado con el letrero.");
        if(model.EmbeddedTextureCount()<2)throw std::runtime_error("El ajolote debe cargar sus dos texturas incrustadas.");
        clip=rig->ClipIndex("AxoGreeting");duration=rig->Duration("AxoGreeting");
        if(duration<7.9f||duration>8.1f)throw std::runtime_error("Duracion inesperada del saludo.");
        ResetInteraction();UpdateGreeting(0);
        std::cout<<"Axolote junto al letrero azul: "<<source.x<<", "<<source.y<<", "<<source.z<<". G: hablar/avanzar dialogo (3 mensajes)."<<std::endl;
    }
    void ResetInteraction(){nearby=false;page=-1;clock=0;}
    bool IsNearby(const glm::vec3& girl) const
    {
        const auto d=girl-position;
        return glm::dot(glm::vec2(d.x,d.z),glm::vec2(d.x,d.z))<=25*worldScale*worldScale&&std::abs(d.y)<=3*worldScale;
    }
    void UpdateProximity(const glm::vec3& girl,bool walking=true)
    {
        const bool insideTrigger=walking&&IsNearby(girl);
        if(!insideTrigger||!nearby)page=-1;
        nearby=insideTrigger;
    }
    bool Interact(const glm::vec3& girl)
    {
        UpdateProximity(girl);if(!nearby)return false;
        if(page<3)++page;return true; // -1: aviso; 0..2: mensajes; 3: cerrado hasta salir.
    }
    void UpdateGreeting(double dt)
    {
        if(std::isfinite(dt)&&dt>=0)clock=std::fmod(clock+dt,static_cast<double>(duration));
        rig->PlayClip(clip,static_cast<float>(clock),true);
    }
    bool DialogueVisible() const{return nearby&&page<3;}
    int MessageIndex() const{return page;}
    void DrawDialogue(int width,int framebufferHeight) const{if(DialogueVisible())dialogue.DrawPanel(static_cast<size_t>(page+1),width,framebufferHeight);}
    glm::vec3 Approach() const{return position+glm::vec3(0,0,3*worldScale);}
    glm::vec3 ViewTarget() const{return glm::mix(position+glm::vec3(0,height*.48f,0),signCenter,.40f);}
    glm::vec3 ViewEye() const{return ViewTarget()+glm::vec3(height*.55f,height*.27f,height*2.75f);}
    void Validate()
    {
        if(!rig->HasDeformingJoint("AxoHand")||!rig->HasDeformingJoint("AxoForearm"))throw std::runtime_error("El saludo no deforma brazo/mano.");
        for(int fps:{30,60,144})
        {
            clock=0;UpdateGreeting(0);const auto initial=rig->JointPosition("AxoHand");auto previous=initial;
            float low=initial.y,high=initial.y,heldMotion=0,waveMotion=0;
            std::array<bool,4> phases{};glm::vec3 held(0),tip(0);bool haveHeld=false;
            for(int f=0;f<fps*16;++f)
            {
                UpdateGreeting(1.0/fps);const auto hand=rig->JointPosition("AxoHand");
                if(!std::isfinite(hand.x)||glm::length(hand-previous)*modelScale>height*.055f)throw std::runtime_error("Salto en saludo del axolote.");
                phases[static_cast<size_t>(CurrentMotionPhase())]=true;
                if(clock>=2.7&&clock<=5.0)
                {
                    const auto finger=rig->JointPosition("AxoFingertip");
                    if(!haveHeld){held=hand;tip=finger;haveHeld=true;}
                    heldMotion=std::max(heldMotion,glm::length(hand-held)*modelScale);
                    waveMotion=std::max(waveMotion,glm::length(finger-tip)*modelScale);
                }
                previous=hand;low=std::min(low,hand.y);high=std::max(high,hand.y);
                const float wrist=rig->JointFlexDegrees("AxoForearm","AxoHand","AxoFingertip");
                if(wrist>22)throw std::runtime_error("La muneca del axolote gira de mas.");
            }
            if(std::any_of(phases.begin(),phases.end(),[](bool visited){return !visited;}))throw std::runtime_error("Falta una fase del saludo.");
            if(heldMotion>height*.005f||waveMotion<height*.005f)throw std::runtime_error("El saludo debe mantener el brazo elevado y mover la mano lateralmente, sin llamar al jugador.");
            if((high-low)*modelScale<height*.12f||glm::length(initial-previous)*modelScale>height*.01f)throw std::runtime_error("El axolote no levanta/regresa el brazo.");
        }
        ResetInteraction();UpdateProximity(Approach());if(!DialogueVisible()||MessageIndex()!=-1)throw std::runtime_error("Falta aviso de G.");
        for(int i=0;i<3;++i){if(!Interact(Approach())||MessageIndex()!=i||!DialogueVisible())throw std::runtime_error("Secuencia de dialogo incorrecta.");}
        Interact(Approach());if(DialogueVisible())throw std::runtime_error("G no cierra el mensaje final.");
        UpdateProximity(position+glm::vec3(worldScale*8,0,0));if(DialogueVisible())throw std::runtime_error("El dialogo no desaparece al salir.");
        Interact(Approach());if(MessageIndex()!=0)throw std::runtime_error("El dialogo no reinicia al regresar.");
        UpdateProximity(position+glm::vec3(0,4*worldScale,0));if(DialogueVisible())throw std::runtime_error("Trigger sin limite de altura.");
        ResetInteraction();UpdateGreeting(0);
        std::cout<<"Axolote OK: solo saludo, elevar/mano lateral/retorno, cuatro fases ciclicas a 30/60/144 FPS, G secuencial 1/2/3, cierre y reinicio al alejarse."<<std::endl;
    }
private:
    SkeletalAnimator* rig=nullptr;NpcDialogue dialogue;int clip=0,page=-1;glm::vec3 signCenter=glm::vec3(0);float worldScale=10,duration=8;double clock=0;bool nearby=false;
};
