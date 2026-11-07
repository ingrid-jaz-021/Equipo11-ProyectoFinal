#pragma once
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "AlphaBillboards.h"
#include "Model.h"

class ZocaloBackdrop
{
public:
    ZocaloBackdrop(AlphaBillboards& renderer,const Model& environment,const GroundSurface& ground,
        const glm::vec3& origin,float scale):renderer(renderer),ground(ground),origin(origin),scale(scale)
    {
        glm::vec3 lo,hi,outerLo,outerHi;
        if(!environment.GetMaterialBounds("Material.006",lo,hi))
            throw std::runtime_error("No se encontro el piso del Zocalo.");
        if(!environment.GetMaterialBounds("Material.013",outerLo,outerHi))
            throw std::runtime_error("No se encontro la calle exterior del Zocalo.");
        // Unir asfalto, borde amarillo y la franja oscura exterior. El piso
        // interior NO define el limite de las fachadas.
        for(const char* material:{"Material.016","Material.017","Material.018","Material.019","Material.020"})
        {
            glm::vec3 edgeLo,edgeHi;
            if(!environment.GetMaterialBounds(material,edgeLo,edgeHi))
                throw std::runtime_error("Falta una franja del perimetro exterior del Zocalo.");
            outerLo=glm::min(outerLo,edgeLo);outerHi=glm::max(outerHi,edgeHi);
        }
        constexpr float clearance=.15f;
        minX=outerLo.x-clearance;maxX=outerHi.x+clearance;
        minZ=outerLo.z-clearance;maxZ=outerHi.z+clearance;
        floor=outerHi.y;center=(glm::vec3((lo.x+hi.x)*.5f,floor,(lo.z+hi.z)*.5f)-origin)*scale;
        glm::vec3 entryLo,entryHi;
        if(!environment.GetMaterialEndBounds("TapeteAserrin1.002",2,1.0f,entryLo,entryHi))
            throw std::runtime_error("No se pudo medir la entrada real al Pasillo.");
        // Reservar TODO el ancho del tapete terminal, con espacio lateral para
        // la huella del personaje. Ningun quad puede invadir este intervalo.
        passageFrom=entryLo.x-1.5f;passageTo=entryHi.x+1.5f;
        passageEndZ=entryHi.z;
        if(passageFrom<=minX||passageTo>=maxX||passageEndZ>=minZ||minZ-passageEndZ>10.0f)
            throw std::runtime_error("El Pasillo no conecta con la cara sur del perimetro exterior.");
        for(int i=0;i<3;++i) houses[i]=renderer.Load("images/Casa"+std::to_string(i+1)+".png");
        church=renderer.Load("images/Iglesia.png");
        AddHouses(0,minX,passageFrom,0); // Sur, tramo izquierdo de la entrada.
        AddHouses(0,passageTo,maxX,1);   // Sur, tramo derecho; centro sin edificios.
        AddHouses(1,minZ,maxZ,1); // Este, normal -X.
        AddHouses(3,minZ,maxZ,2); // Oeste, normal +X.
        // UNICA iglesia: centro de la cara norte, mirando hacia la plaza.
        const float churchWidth=30.0f*renderer.Aspect(church),middle=(minX+maxX)*.5f;
        AddHouses(2,minX,middle-churchWidth*.5f,1);
        Add(2,middle,church,30.0f,true);
        AddHouses(2,middle+churchWidth*.5f,maxX,2);
        Validate();
        std::cout<<"Fachadas Zocalo: "<<records.size()-1<<" casas alternadas, 1 iglesia, fuera de calle y borde oscuro."<<std::endl;
        std::cout<<"Perimetro fuente X: "<<minX<<".."<<maxX<<"; Z: "<<minZ<<".."<<maxZ
            <<". Acceso Pasillo libre X: "<<passageFrom<<".."<<passageTo<<" (ancho "<<passageTo-passageFrom<<")."<<std::endl;
    }
    const glm::vec3& Center() const {return center;}
    glm::vec3 PassageTarget() const
    {return(glm::vec3((passageFrom+passageTo)*.5f,floor+4.0f,passageEndZ)-origin)*scale;}
    glm::vec3 PassageEye(bool fromPlaza) const
    {return(glm::vec3((passageFrom+passageTo)*.5f,floor+3.0f,fromPlaza?minZ+18.0f:passageEndZ-20.0f)-origin)*scale;}
    glm::vec3 SideTarget(int side) const
    {
        const float along=side==0?(passageFrom+passageTo)*.5f:side%2?(minZ+maxZ)*.5f:(minX+maxX)*.5f;
        glm::vec3 p=SourcePosition(side,along);
        p.y+=side==2?15.0f:6.0f;return(p-origin)*scale;
    }
    void Validate() const
    {
        std::array<int,4> counts{};std::array<int,3> variety{};int churches=0;
        for(const auto& record:records)
        {
            ++counts[record.side];churches+=record.church?1:0;
            if(!record.church) for(int i=0;i<3;++i) if(record.texture==houses[i]) ++variety[i];
            const glm::vec3 p=(SourcePosition(record.side,record.along)-origin)*scale;
            const glm::vec3 inward=glm::normalize(glm::vec3(center.x-p.x,0,center.z-p.z));
            const float yaw=Yaw(record.side);
            if(glm::dot(inward,glm::vec3(std::sin(yaw),0,std::cos(yaw)))<.65f)
                throw std::runtime_error("Fachada orientada hacia fuera de la plaza.");
            if(record.side==0&&record.along+record.width*.5f>passageFrom+.001f&&
                record.along-record.width*.5f<passageTo-.001f)
                throw std::runtime_error("Una fachada invade el acceso al Pasillo.");
        }
        if(churches!=1) throw std::runtime_error("La iglesia debe aparecer exactamente una vez.");
        for(int count:counts) if(count<3) throw std::runtime_error("Perimetro de fachadas incompleto.");
        for(int count:variety) if(count<2) throw std::runtime_error("Faltan variantes repetidas de casas.");
        for(int side=0;side<4;++side)
        {
            auto ordered=records;
            ordered.erase(std::remove_if(ordered.begin(),ordered.end(),[side](const Record& r){return r.side!=side;}),ordered.end());
            std::sort(ordered.begin(),ordered.end(),[](const Record& a,const Record& b){return a.along<b.along;});
            const float from=side%2?minZ:minX,to=side%2?maxZ:maxX;
            const std::vector<glm::vec2> spans=side==0?
                std::vector<glm::vec2>{{from,passageFrom},{passageTo,to}}:std::vector<glm::vec2>{{from,to}};
            for(const auto& span:spans)
            {
                float end=span.x;
                for(const auto& r:ordered)
                {
                    const float left=r.along-r.width*.5f,right=r.along+r.width*.5f;
                    if(right<=span.x+.001f||left>=span.y-.001f)continue;
                    if(left>end+.01f) throw std::runtime_error("Hueco no autorizado entre fachadas del Zocalo.");
                    end=std::max(end,right);
                }
                if(end<span.y-.01f) throw std::runtime_error("Extremo del perimetro sin fachada.");
            }
        }
        // Barrido de una huella de personaje por todo el ancho de la conexion.
        constexpr float radius=.4f;
        for(int i=0;i<=32;++i)
        {
            const float x=glm::mix(passageFrom+radius,passageTo-radius,i/32.0f);
            for(const auto& r:records) if(r.side==0&&x+radius>r.along-r.width*.5f+.001f&&
                x-radius<r.along+r.width*.5f-.001f)
                throw std::runtime_error("El volumen de paso hacia el Pasillo esta bloqueado.");
        }
    }
private:
    struct Record {int side;float along,width;size_t texture;bool church;};
    AlphaBillboards& renderer;const GroundSurface& ground;
    glm::vec3 origin,center;float scale,minX,maxX,minZ,maxZ,floor;
    float passageFrom=0,passageTo=0,passageEndZ=0;
    std::array<size_t,3> houses{};size_t church=0;
    std::vector<Record> records;
    float Yaw(int side) const
    {
        const float degrees[4]={0,-90,180,90};return glm::radians(degrees[side]);
    }
    glm::vec3 SourcePosition(int side,float along) const
    {
        if(side==0)return{along,floor,minZ};if(side==1)return{maxX,floor,along};
        if(side==2)return{along,floor,maxZ};return{minX,floor,along};
    }
    void Add(int side,float along,size_t texture,float height,bool isChurch=false)
    {
        auto base=SourcePosition(side,along);
        float support=floor;ground.Sample(base.x,base.z,floor,support);base.y=support-.10f;
        renderer.Add(texture,(base-origin)*scale,Yaw(side),height*scale);
        records.push_back({side,along,height*renderer.Aspect(texture),texture,isChurch});
    }
    void AddHouses(int side,float from,float to,int variation)
    {
        const int count=std::max(1,static_cast<int>(std::ceil((to-from)/28.0f)));
        float aspectSum=0,overlapSum=0;
        for(int i=0;i<count;++i)
        {
            const float aspect=renderer.Aspect(houses[(i+variation)%3]);aspectSum+=aspect;
            if(i+1<count)overlapSum+=.025f*std::min(aspect,renderer.Aspect(houses[(i+1+variation)%3]));
        }
        // Solape SOLO entre vecinos: el primer/ultimo quad termina exactamente
        // en el tramo permitido y nunca se extiende sobre el hueco del Pasillo.
        const float height=(to-from)/(aspectSum-overlapSum);
        float cursor=from;
        for(int i=0;i<count;++i)
        {
            const size_t texture=houses[(i+variation)%3];const float width=height*renderer.Aspect(texture);
            Add(side,cursor+width*.5f,texture,height);cursor+=width;
            if(i+1<count)cursor-=height*.025f*std::min(renderer.Aspect(texture),renderer.Aspect(houses[(i+1+variation)%3]));
        }
    }
};
