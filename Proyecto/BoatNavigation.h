#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include "CompanionController.h"
#include "WaterSurface.h"

class BoatNavigation
{
public:
    struct Area{glm::vec3 min,max,dockMin,dockMax;}; // Coordenadas originales del escenario.
    enum class Interaction{TooFar,Boarded,Disembarked,NoLanding};
    BoatNavigation(const Area& area,GroundSurface& ground,const glm::vec3& origin,float scale,bool createGpu=true)
        :area(area),ground(ground),origin(origin),scale(scale)
    {
        waterY=(area.min.y-origin.y)*scale;
        mooring=glm::vec3(area.dockMax.x+HalfLength+.65f,area.min.y,(area.dockMin.z+area.dockMax.z)*.5f);
        AddShore(createGpu);if(createGpu)BuildMarker();Reset();
        if(!CanFloat(position,yaw))throw std::runtime_error("El amarre inicial del bote invade el muelle.");
    }
    void Reset(){navigating=false;position=World(mooring);yaw=HalfPi;Anchor();}
    bool IsNavigating() const{return navigating;}
    const glm::vec3& Position() const{return position;}
    float Yaw() const{return yaw;}
    const Area& Bounds() const{return area;}
    glm::vec3 DockSpot() const
    {return World({area.dockMax.x-1.4f,area.dockMax.y,(area.dockMin.z+area.dockMax.z)*.5f});}
    void Anchor(){speed=turnRate=0;}
    Interaction Interact(CharacterController& girl,CompanionController& dog)
    {
        if(!navigating)
        {
            const auto local=glm::vec3(glm::inverse(Pose())*glm::vec4(girl.position,1))/scale;
            const glm::vec2 delta=glm::max(glm::abs(glm::vec2(local.x,local.z))-glm::vec2(HalfWidth,HalfLength),glm::vec2(0));
            if(glm::length(delta)>2.5f||std::abs(girl.position.y-waterY)>4*scale)return Interaction::TooFar;
            navigating=true;Anchor();girl.PreparePassenger();dog.PreparePassenger();
            PlacePassengers(girl,dog);return Interaction::Boarded;
        }
        glm::vec3 landing,pet;float heading;
        if(!FindLanding(landing,pet,heading))return Interaction::NoLanding;
        navigating=false;Anchor();girl.SetFrozen(false);girl.Reset();girl.position=landing;girl.yaw=heading;
        dog.ResetBeside(girl,scale);dog.position=pet;
        return Interaction::Disembarked;
    }
    void Update(glm::vec2 input,bool fast,float dt)
    {
        if(!navigating)return; // El bote queda anclado al desembarcar.
        input=glm::clamp(input,glm::vec2(-1),glm::vec2(1));
        const int steps=std::max(1,static_cast<int>(std::ceil(dt*120)));
        for(int step=0;step<steps;++step)Tick(input,fast,dt/steps);
        position.y=waterY; // Flotacion estricta: sin gravedad, cabeceo ni acumulacion en Y.
    }
    void PlacePassengers(CharacterController& girl,CompanionController& dog) const
    {
        if(!navigating)return;
        const auto pose=Pose();
        girl.position=glm::vec3(pose*glm::vec4(glm::vec3(.75f,.205f,-.10f)*scale,1));girl.yaw=yaw;
        dog.position=glm::vec3(pose*glm::vec4(glm::vec3(-.95f,.205f,-.65f)*scale,1));dog.yaw=yaw;
        // Solo cambia su transformacion de pasajeros; las matrices de huesos permanecen fijas.
    }
    bool GuardWalking(CharacterController& actor,const glm::vec3& previous) const
    {
        if(navigating)return true;
        const auto p=Source(actor.position);
        if(p.x<area.min.x||p.x>area.max.x||p.z<area.min.z||p.z>area.max.z)return true;
        float y;
        // Caminando se permite apoyo parcial y la MISMA altura de paso que el
        // controlador. FullSupport exige planitud y se reserva al desembarque:
        // aplicarlo aqui convertia el desnivel Pasillo/muelle en una pared.
        const float step=std::max(actor.height/scale*.8f,MovementSettings::CharacterHeight*.8f);
        if(ground.SampleCharacter(p.x,p.z,p.y,y,actor.height/scale*.12f,step)&&y>area.min.y+.1f)return true;
        actor.position=previous;actor.velocity=glm::vec3(0);return false;
    }
    glm::mat4 ModelMatrix() const{return glm::scale(Pose(),glm::vec3(scale));}
    glm::mat4 MarkerMatrix(double seconds) const
    {
        const float bob=.55f*std::sin(static_cast<float>(seconds)*1.8f);
        return glm::scale(glm::translate(glm::mat4(1),position+glm::vec3(0,(4.9f+bob)*scale,0)),glm::vec3(scale));
    }
    void DrawMarker(const Shader& shader,const glm::mat4& view,const glm::mat4& projection,double seconds) const
    {
        if(!marker)return;
        const auto model=MarkerMatrix(seconds);
        if(!TransformedBoundsVisible(ExtractFrustum(projection*view),{-.9f,0,-.9f},{.9f,1.9f,.9f},model))
        {++RenderMetrics::culled;return;}
        shader.Use();const auto normal=glm::mat3(1/scale);
        glUniformMatrix4fv(shader.Uniform("model"),1,GL_FALSE,&model[0][0]);
        glUniformMatrix4fv(shader.Uniform("view"),1,GL_FALSE,&view[0][0]);
        glUniformMatrix4fv(shader.Uniform("projection"),1,GL_FALSE,&projection[0][0]);
        glUniformMatrix3fv(shader.Uniform("normalMatrix"),1,GL_FALSE,&normal[0][0]);
        marker->Draw(shader);
    }
    void DrawShore(const Shader& shader,const glm::mat4& view,const glm::mat4& projection) const
    {
        if(!shore)return;
        const auto model=glm::scale(glm::translate(glm::mat4(1),-origin*scale),glm::vec3(scale));
        if(!TransformedBoundsVisible(ExtractFrustum(projection*view),area.min,area.max+glm::vec3(0,1,0),model))
        {++RenderMetrics::culled;return;}
        shader.Use();
        const auto normal=glm::transpose(glm::inverse(glm::mat3(model)));
        glUniformMatrix4fv(shader.Uniform("model"),1,GL_FALSE,&model[0][0]);
        glUniformMatrix4fv(shader.Uniform("view"),1,GL_FALSE,&view[0][0]);
        glUniformMatrix4fv(shader.Uniform("projection"),1,GL_FALSE,&projection[0][0]);
        glUniformMatrix3fv(shader.Uniform("normalMatrix"),1,GL_FALSE,&normal[0][0]);
        shore->Draw(shader);
    }
    // Validaciones usan la misma geometria/colisiones, sin modificar controles reales.
    void Validate(CharacterController& girl,CompanionController& dog);
    void ValidateDockAccess(CharacterController& girl,CompanionController& dog)
    {
        for(int fps:{30,60,144})
        {
            Reset();girl.SetFrozen(false);girl.Reset();
            const float z=(area.dockMin.z+area.dockMax.z)*.5f,start=area.dockMin.x-12;
            float y;
            if(!ground.SampleCharacter(start,z,area.min.y+4.5f,y))throw std::runtime_error("No existe el piso de acceso al muelle.");
            girl.position=World({start,y,z});dog.ResetBeside(girl,scale);
            int rejected=0,frames=0;
            while(Source(girl.position).x<area.dockMax.x-1.4f&&frames++<fps*25)
            {
                const auto before=girl.position,petBefore=dog.position;
                girl.Update({0,1},{1,0,0},false,1.0f/fps,scale);
                if(!GuardWalking(girl,before))++rejected;
                girl.ConformToGround(ground,origin,scale,1.0f/fps);
                dog.UpdateCompanion(girl,&ground,origin,scale,1.0f/fps);GuardWalking(dog,petBefore);
            }
            if(rejected||Source(girl.position).x<area.dockMax.x-1.4f||Interact(girl,dog)!=Interaction::Boarded)
                throw std::runtime_error("El recorrido a pie Pasillo/muelle/trigger queda bloqueado.");
        }
        Reset();girl.SetFrozen(false);girl.Reset();dog.ResetBeside(girl,scale);
        std::cout<<"Acceso al bote OK: recorrido real a pie desde el Pasillo hasta E, sin bloqueos, a 30/60/144 FPS."<<std::endl;
    }
private:
    static constexpr float HalfWidth=2.3f,HalfLength=4.0f,HalfPi=1.57079632679f,ShoreWidth=4.0f;
    Area area;GroundSurface& ground;glm::vec3 origin,mooring,position;float scale,waterY,yaw=0,speed=0,turnRate=0;
    bool navigating=false;std::unique_ptr<Mesh> shore,marker;
    void BuildMarker()
    {
        const std::array<glm::vec3,5> p={glm::vec3(0),{-.9f,1.9f,-.9f},{.9f,1.9f,-.9f},{.9f,1.9f,.9f},{-.9f,1.9f,.9f}};
        std::vector<Vertex> vertices;std::vector<GLuint> indices;
        for(const auto& face:std::array<std::array<int,3>,6>{{{0,2,1},{0,3,2},{0,4,3},{0,1,4},{1,2,3},{1,3,4}}})
        {
            const auto normal=glm::normalize(glm::cross(p[face[1]]-p[face[0]],p[face[2]]-p[face[0]]));
            for(int index:face){indices.push_back(static_cast<GLuint>(vertices.size()));vertices.push_back({p[index],normal,glm::vec2(0)});}
        }
        MeshMaterial bright;bright.diffuseColor={1,.76f,.02f};bright.emissiveColor={1.4f,.75f,.01f};
        marker=std::make_unique<Mesh>(vertices,indices,bright,glm::vec3(-.9f,0,-.9f),glm::vec3(.9f,1.9f,.9f));
    }
    glm::vec3 World(const glm::vec3& p) const{return (p-origin)*scale;}
    glm::vec3 Source(const glm::vec3& p) const{return p/scale+origin;}
    glm::mat4 Pose() const{return glm::rotate(glm::translate(glm::mat4(1),position),yaw,glm::vec3(0,1,0));}
    bool CanFloat(const glm::vec3& p,float angle) const
    {
        const auto center=Source(p);const float c=std::cos(angle),s=std::sin(angle);
        const float ex=std::abs(c)*HalfWidth+std::abs(s)*HalfLength,ez=std::abs(s)*HalfWidth+std::abs(c)*HalfLength;
        if(center.x-ex<area.min.x+.1f||center.x+ex>area.max.x-.1f||center.z-ez<area.min.z+.1f||center.z+ez>area.max.z-.1f)return false;
        for(float x:{-2.25f,-1.1f,0.0f,1.1f,2.25f})for(float z:{-3.65f,-2.0f,0.0f,2.0f,3.8f})
        {
            float y;const glm::vec2 point(center.x+c*x+s*z,center.z-s*x+c*z);
            if(ground.SampleCharacter(point.x,point.y,area.min.y+4.5f,y,0,0)&&y>area.min.y+.1f)return false;
        }
        return true;
    }
    void Tick(glm::vec2 input,bool fast,float dt)
    {
        const float targetSpeed=input.y*(input.y<0?4.0f:fast?12.0f:8.0f);
        const float steering=-input.x*1.35f,angularDecay=std::exp(-5.0f*dt),speedDecay=std::exp(-3.0f*dt);
        const float angle=steering*dt+(turnRate-steering)*(1-angularDecay)/5;
        turnRate=steering+(turnRate-steering)*angularDecay;
        if(CanFloat(position,yaw+angle))yaw=std::remainder(yaw+angle,6.28318530718f);else turnRate=0;
        const float distance=targetSpeed*dt+(speed-targetSpeed)*(1-speedDecay)/3;
        speed=targetSpeed+(speed-targetSpeed)*speedDecay;
        const glm::vec3 delta(std::sin(yaw)*distance*scale,0,std::cos(yaw)*distance*scale);
        auto next=position+delta;
        // Limitar TODO el casco, incluida su orientacion, al rectangulo navegable.
        const float ex=(std::abs(std::cos(yaw))*HalfWidth+std::abs(std::sin(yaw))*HalfLength+.11f)*scale;
        const float ez=(std::abs(std::sin(yaw))*HalfWidth+std::abs(std::cos(yaw))*HalfLength+.11f)*scale;
        next.x=glm::clamp(next.x,World(area.min).x+ex,World(area.max).x-ex);
        next.z=glm::clamp(next.z,World(area.min).z+ez,World(area.max).z-ez);
        next.y=waterY;
        if(CanFloat(next,yaw))position=next;
        else
        {
            const glm::vec3 slideX(next.x,waterY,position.z),slideZ(position.x,waterY,next.z);
            if(CanFloat(slideX,yaw))position=slideX;
            else if(CanFloat(slideZ,yaw))position=slideZ;
            speed=0;
        }
    }
    bool FullSupport(const glm::vec2& p,float radius,float& height) const
    {
        float lowest=1e9f,highest=-1e9f;
        for(const auto& offset:std::array<glm::vec2,5>{glm::vec2(0),{radius,0},{-radius,0},{0,radius},{0,-radius}})
        {
            float y;
            if(!ground.SampleCharacter(p.x+offset.x,p.y+offset.y,area.min.y+4.5f,y,0,0)||y<area.min.y+.1f)return false;
            lowest=std::min(lowest,y);highest=std::max(highest,y);
        }
        height=highest;return highest-lowest<.45f;
    }
    bool FindLanding(glm::vec3& landing,glm::vec3& pet,float& heading) const
    {
        const auto center=Source(position);
        for(float r=3;r<=8.01f;r+=.25f)for(int i=0;i<64;++i)
        {
            const float angle=i*6.28318530718f/64;const glm::vec2 direction(std::sin(angle),std::cos(angle));
            const glm::vec2 point=glm::vec2(center.x,center.z)+direction*r;
            const glm::vec2 petPoint=point+glm::vec2(-direction.y,direction.x)*1.8f-direction*.8f;
            float y,petY;
            if(!FullSupport(point,MovementSettings::CharacterHeight*.12f,y)||!FullSupport(petPoint,.25f,petY))continue;
            landing=World({point.x,y,point.y});pet=World({petPoint.x,petY,petPoint.y});heading=angle;
            return true;
        }
        return false;
    }
    void AddShore(bool createGpu)
    {
        std::vector<Vertex> vertices;std::vector<GLuint> indices;
        const float top=area.min.y+.65f;
        const auto box=[&](float x0,float z0,float x1,float z1)
        {
            const std::array<glm::vec3,8> p={glm::vec3(x0,top-.35f,z0),{x1,top-.35f,z0},{x1,top-.35f,z1},{x0,top-.35f,z1},
                {x0,top,z0},{x1,top,z0},{x1,top,z1},{x0,top,z1}};
            for(const auto& face:std::array<std::array<int,4>,6>{{{0,1,2,3},{4,7,6,5},{0,4,5,1},{1,5,6,2},{2,6,7,3},{3,7,4,0}}})
            {
                const auto normal=glm::normalize(glm::cross(p[face[1]]-p[face[0]],p[face[2]]-p[face[0]]));
                const auto first=static_cast<GLuint>(vertices.size());
                for(int i:face)vertices.push_back({p[i],normal,glm::vec2(0)});
                indices.insert(indices.end(),{first,first+1,first+2,first,first+2,first+3});
            }
            ground.Add(p[4],p[5],p[6]);ground.Add(p[4],p[6],p[7]);
        };
        box(area.min.x,area.min.z,area.max.x,area.min.z+ShoreWidth);
        box(area.min.x,area.max.z-ShoreWidth,area.max.x,area.max.z);
        box(area.max.x-ShoreWidth,area.min.z+ShoreWidth,area.max.x,area.max.z-ShoreWidth);
        box(area.min.x,area.min.z+ShoreWidth,area.min.x+ShoreWidth,area.dockMin.z-1.5f);
        box(area.min.x,area.dockMax.z+1.5f,area.min.x+ShoreWidth,area.max.z-ShoreWidth);
        if(createGpu)
        {
            MeshMaterial material;material.diffuseColor={.36f,.24f,.14f};
            shore=std::make_unique<Mesh>(vertices,indices,material,area.min,area.max+glm::vec3(0,1,0));
        }
    }
};

inline void BoatNavigation::Validate(CharacterController& girl,CompanionController& dog)
{
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    glm::vec3 reference(0);
    const float high=MarkerMatrix(3.14159265359/3.6)[3].y,low=MarkerMatrix(3*3.14159265359/3.6)[3].y;
    require(std::abs((high-low)/scale-1.1f)<.001f&&std::abs(position.y-waterY)<.001f,"La flecha no oscila independientemente del barco.");
    for(int fps:{30,60,144})
    {
        Reset();girl.SetFrozen(false);girl.Reset();girl.position=DockSpot();dog.ResetBeside(girl,scale);
        require(Interact(girl,dog)==Interaction::Boarded,"E no embarca desde el muelle.");
        const auto humanPose=girl.Pose(),dogPose=dog.Pose();
        require(girl.IsFrozen()&&dog.IsFrozen()&&!dog.TriggerSpecial(),"Los pasajeros conservan controles de caminata.");
        for(int i=0;i<fps*2;++i){Update({0,1},false,1.0f/fps);PlacePassengers(girl,dog);}
        require(position.y==waterY,"El bote no mantiene la altura exacta del agua.");
        require(std::memcmp(humanPose.data(),girl.Pose().data(),humanPose.size()*sizeof(glm::mat4))==0&&
            std::memcmp(dogPose.data(),dog.Pose().data(),dogPose.size()*sizeof(glm::mat4))==0,"Los pasajeros se animan navegando.");
        const auto passengerLocal=glm::vec3(glm::inverse(Pose())*glm::vec4(girl.position,1))/scale;
        require(glm::length(passengerLocal-glm::vec3(.75f,.205f,-.10f))<.001f,"El pasajero no sigue la rotacion del casco.");
        if(fps==30)reference=position;else require(glm::length(position-reference)<.03f*scale,"La navegacion depende de FPS.");
        require(Interact(girl,dog)==Interaction::NoLanding,"Se permite desembarcar en agua profunda.");
        const float before=yaw;
        for(int i=0;i<fps;++i){Update({1,1},false,1.0f/fps);PlacePassengers(girl,dog);}
        require(std::abs(yaw-before)>.5f&&CanFloat(position,yaw),"El timon no gira suavemente o atraviesa el suelo.");
        // Cuatro orillas: navegar hasta el limite, desembarcar en soporte real,
        // dejar el bote anclado y volver a embarcar desde ese mismo punto.
        for(int side=0;side<4;++side)
        {
            const float x=(area.min.x+area.max.x)*.5f,z=area.max.z-35;
            const glm::vec3 sites[4]={{x,area.min.y,area.min.z+10},{area.max.x-10,area.min.y,z},
                {x,area.min.y,area.max.z-10},{area.min.x+10,area.min.y,z}};
            position=World(sites[side]);yaw=side==0?2*HalfPi:side==1?HalfPi:side==2?0:-HalfPi;
            Anchor();navigating=true;
            for(int i=0;i<fps*3;++i)Update({0,1},true,1.0f/fps);
            require(CanFloat(position,yaw)&&position.y==waterY,"El casco sale del lago o invade la orilla.");
            require(Interact(girl,dog)==Interaction::Disembarked,"No se puede desembarcar en una orilla.");
            require(!girl.IsFrozen()&&!dog.IsFrozen(),"No se recuperan los controles al desembarcar.");
            const auto anchored=position;Update({1,1},true,1.0f);
            require(glm::length(position-anchored)==0,"El bote deriva sin pasajeros.");
            require(Interact(girl,dog)==Interaction::Boarded,"No se puede volver a embarcar desde la orilla.");
        }
        Reset();girl.SetFrozen(false);girl.Reset();girl.position=DockSpot();dog.ResetBeside(girl,scale);
        require(Interact(girl,dog)==Interaction::Boarded&&Interact(girl,dog)==Interaction::Disembarked,
            "El ciclo subir/bajar no funciona en el muelle.");
    }
    Reset();girl.SetFrozen(false);girl.Reset();dog.ResetBeside(girl,scale);
    std::cout<<"Bote OK: E, pasajeros estaticos, timon, Y fija, limites, cuatro orillas y reembarque a 30/60/144 FPS."<<std::endl;
}
