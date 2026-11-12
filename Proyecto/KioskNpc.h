#pragma once
#include <array>
#include <cmath>
#include "CharacterController.h"
#include "Model.h"

class KioskNpc:public CharacterController
{
public:
    enum class State{Idle,Appear,Eating,Finish};
    void ConfigureNpc(Model& model,const Model& bread,const Model* environment,const glm::vec3& origin,float scale)
    {
        rig=model.Animator();if(!rig)throw std::runtime_error("NPC sin armadura.");Attach(rig);SetFrozen(true);
        clips={rig->ClipIndex("Idle"),rig->ClipIndex("ReachBasket"),rig->ClipIndex("Appear"),
            rig->ClipIndex("RaiseBread"),rig->ClipIndex("Eat"),rig->ClipIndex("ReturnHand")};
        unit=glm::length(rig->JointPosition("NpcScaleTop")-rig->JointPosition("NpcScaleBottom"));
        anchor=rig->JointPosition("NpcSeatAnchor");
        Model::BenchSeat bench{{origin.x,origin.y+1.0f,origin.z+12},3.14159265359f,origin.y};
        if(environment)
        {
            const auto& seats=environment->BenchSeats();if(seats.empty())throw std::runtime_error("No hay bancas para el NPC.");
            bench=*std::min_element(seats.begin(),seats.end(),[&](const auto& a,const auto& b)
            {const auto da=glm::vec2(a.anchor.x-origin.x,a.anchor.z-origin.z),db=glm::vec2(b.anchor.x-origin.x,b.anchor.z-origin.z);return glm::dot(da,da)<glm::dot(db,db);});
        }
        const float seatToFeet=anchor.y-model.GetMinBounds().y;
        if(unit<=0||seatToFeet<=.05f*unit)throw std::runtime_error("Pose sentada o escala del NPC incorrecta.");
        seat=(bench.anchor-origin)*scale;yaw=bench.yaw;
        modelScale=(bench.anchor.y-bench.floor)*scale/seatToFeet;height=unit*modelScale;
        position=glm::vec3(seat.x,(bench.floor-origin.y)*scale,seat.z);
        breadAnchor={ (bread.GetMinBounds().x+bread.GetMaxBounds().x)*.5f,bread.GetMinBounds().y,(bread.GetMinBounds().z+bread.GetMaxBounds().z)*.5f};
        breadDiameter=std::max(bread.GetMaxBounds().x-bread.GetMinBounds().x,bread.GetMaxBounds().z-bread.GetMinBounds().z);
        if(breadDiameter<=0)throw std::runtime_error("El pan no tiene geometria.");
        ResetCycle();
        std::cout<<"NPC sentado en banca: "<<bench.anchor.x<<", "<<bench.anchor.y<<", "<<bench.anchor.z
            <<"; altura de referencia "<<height/scale<<"; ciclo Idle/Aparicion/Comiendo/Fin."<<std::endl;
    }
    void ResetCycle(){state=State::Idle;time=0;cycles=0;ApplyPose();}
    void UpdateNpc(double dt)
    {
        if(!std::isfinite(dt)||dt<0)return;
        time+=dt;
        while(time>=Duration(state))
        {
            time-=Duration(state);
            state=state==State::Finish?State::Idle:static_cast<State>(static_cast<int>(state)+1);
            if(state==State::Idle)++cycles;
        }
        ApplyPose();
    }
    bool BreadVisible() const{return (state==State::Appear||state==State::Eating)&&BreadAmount()>.05f;}
    float BreadAmount() const
    {return state==State::Eating?1-.95f*glm::clamp(static_cast<float>((time-1.2)/3.3),0.0f,1.0f):state==State::Appear?1.0f:0.0f;}
    glm::mat4 WorldMatrix() const
    {
        auto matrix=glm::rotate(glm::translate(glm::mat4(1),seat),yaw,glm::vec3(0,1,0));
        return glm::translate(glm::scale(matrix,glm::vec3(modelScale)),-anchor);
    }
    glm::mat4 GripMatrix() const
    {
        auto hand=rig->JointMatrix("NpcHand.R");
        for(int i=0;i<3;++i)hand[i]=glm::vec4(glm::normalize(glm::vec3(hand[i]))*unit,0);
        return WorldMatrix()*glm::translate(hand,glm::vec3(0,.045f,.025f));
    }
    glm::mat4 BreadMatrix() const
    {
        const auto grip=GripMatrix();auto matrix=glm::mat4(1);
        for(int i=0;i<3;++i)matrix[i]=glm::vec4(glm::normalize(glm::vec3(grip[i])),0);
        matrix[3]=grip[3];matrix=glm::rotate(matrix,1.57079632679f,glm::vec3(1,0,0));
        matrix=glm::scale(matrix,glm::vec3(height*.12f/breadDiameter*BreadAmount()));
        return glm::translate(matrix,-breadAnchor);
    }
    glm::mat4 BasketMatrix() const
    {
        return glm::scale(glm::translate(WorldMatrix(),glm::vec3(-.26f,.485f,.030f)*unit),glm::vec3(unit*.14f));
    }
    glm::vec3 DialogueApproach(float scale) const
    {
        const glm::vec3 front(std::sin(yaw),0,std::cos(yaw)),right(std::cos(yaw),0,-std::sin(yaw));
        return position+(front*3.0f-right*2.0f)*scale;
    }
    glm::vec3 ViewTarget() const{return seat+glm::vec3(0,height*.20f,0);}
    glm::vec3 ViewEye() const
    {
        const auto front=glm::vec3(std::sin(yaw),0,std::cos(yaw)),right=glm::vec3(std::cos(yaw),0,-std::sin(yaw));
        return ViewTarget()+front*(height*1.45f)+right*(height*.80f)+glm::vec3(0,height*.45f,0);
    }
    void Validate()
    {
        for(const char* joint:{"NpcForearm.R","NpcShin.R","NpcHand.R"})
            if(!rig->HasDeformingJoint(joint))throw std::runtime_error("NPC sin pesos articulados suficientes.");
        if(std::abs(BasketMatrix()[3].y-seat.y)>.001f)throw std::runtime_error("La canasta no esta apoyada en la banca.");
        for(int fps:{30,60,144})
        {
            ResetCycle();std::array<bool,4> visited{};float previous=1;
            for(int f=0;f<fps*23;++f)
            {
                UpdateNpc(1.0/fps);visited[static_cast<int>(state)]=true;
                const float wrist=rig->JointFlexDegrees("NpcForearm.R","NpcHand.R","NpcHandTip.R");
                const float elbow=rig->JointFlexDegrees("NpcUpperArm.R","NpcForearm.R","NpcHand.R");
                if(!std::isfinite(wrist)||wrist>31||elbow<17||elbow>133)throw std::runtime_error("Codo o muneca del NPC fuera de limites anatomicos.");
                if(state==State::Appear)
                {
                    const auto rim=BasketMatrix()*glm::vec4(0,.645f,0,1);
                    const auto grip=GripMatrix()[3];
                    if(glm::length(glm::vec2(grip.x-rim.x,grip.z-rim.z))>height*.11f||std::abs(grip.y-rim.y)>height*.1f)
                        throw std::runtime_error("El NPC toma el pan lejos de la canasta.");
                }
                if(state==State::Eating)
                {
                    const float amount=BreadAmount();if(amount>previous+.0001f)throw std::runtime_error("El pan crece durante la comida.");previous=amount;
                    const auto hand=WorldMatrix()*rig->JointMatrix("NpcHand.R");
                    if(glm::length(glm::vec3(GripMatrix()[3]-hand[3]))>height*.1f)throw std::runtime_error("El pan se separa de la mano.");
                }
                else previous=1;
                if((state==State::Idle||state==State::Finish)&&BreadVisible())throw std::runtime_error("El pan se dibuja fuera de su estado.");
            }
            for(bool value:visited)if(!value)throw std::runtime_error("Falta un estado del NPC.");
            if(cycles!=2)throw std::runtime_error("El ciclo del NPC depende de los FPS.");
        }
        ResetCycle();std::cout<<"NPC OK: canasta sobre banca, toma del pan, muneca <=30 grados, cuatro estados y dos ciclos a 30/60/144 FPS."<<std::endl;
    }
private:
    SkeletalAnimator* rig=nullptr;State state=State::Idle;double time=0;unsigned cycles=0;
    glm::vec3 seat=glm::vec3(0),anchor=glm::vec3(0),breadAnchor=glm::vec3(0);float unit=1,breadDiameter=1;
    std::array<int,6> clips{};
    static double Duration(State s){return s==State::Idle?4.2:s==State::Appear?.6:s==State::Eating?4.5:1.4;}
    void ApplyPose()
    {
        if(!rig)return;
        if(state==State::Idle)rig->PlayClip(clips[time<3?0:1],static_cast<float>(time<3?time:time-3),time<3);
        else if(state==State::Appear)rig->PlayClip(clips[2],static_cast<float>(time));
        else if(state==State::Eating)rig->PlayClip(clips[time<1.2?3:4],static_cast<float>(time<1.2?time:time-1.2));
        else rig->PlayClip(clips[5],static_cast<float>(time));
    }
};
