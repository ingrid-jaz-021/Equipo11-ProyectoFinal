#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <iostream>
#include <string>
#include "Model.h"
#pragma comment(lib,"winmm.lib")

class AmbientAudio
{
public:
    enum class Zone { Silent, Corridor, Lagoon };
    explicit AmbientAudio(bool silentTest=false):volume(silentTest?0:380)
    {
        corridorOpen=Open(L"audio/Pasillo.mp3",L"festival_corridor");
        lagoonOpen=Open(L"audio/Laguna.mp3",L"festival_lagoon");
        std::cout<<"Audio: Pasillo.mp3 / Laguna.mp3; bucle por zonas, volumen 38%."<<std::endl;
    }
    ~AmbientAudio(){StopBoth();if(corridorOpen)Send(L"close festival_corridor");if(lagoonOpen)Send(L"close festival_lagoon");}
    AmbientAudio(const AmbientAudio&)=delete;
    AmbientAudio& operator=(const AmbientAudio&)=delete;
    void Configure(const Model& scene,const glm::vec3& sourceOrigin,float worldScale,const glm::vec3& lakeLo,const glm::vec3& lakeHi)
    {
        origin=sourceOrigin;scale=worldScale;lakeMin=lakeLo;lakeMax=lakeHi;
        glm::vec3 lo,hi;
        if(!scene.GetMaterialBounds("Material.020",lo,hi))throw std::runtime_error("Audio: falta el limite del Zocalo.");
        plazaLo=lo;plazaHi=hi;
        if(!scene.GetMaterialBounds("Material.001",corridorLo,corridorHi))throw std::runtime_error("Audio: falta el Pasillo.");
        configured=true;
    }
    Zone At(const glm::vec3& world,bool sailing=false) const
    {
        if(sailing)return Zone::Lagoon;
        if(!configured)return Zone::Silent;
        const auto p=world/scale+origin;
        if(p.x>=lakeMin.x-12&&p.x<=lakeMax.x+8&&p.z>=lakeMin.z-8&&p.z<=lakeMax.z+8)return Zone::Lagoon;
        if(p.x>=plazaLo.x&&p.x<=plazaHi.x&&p.z>=plazaLo.z-.25f&&p.z<=plazaHi.z)return Zone::Silent;
        if(p.x>=corridorLo.x-1&&p.x<=corridorHi.x+1&&p.z>=corridorLo.z-1&&p.z<plazaLo.z)return Zone::Corridor;
        return Zone::Silent;
    }
    void Update(const glm::vec3& world,bool sailing){Switch(At(world,sailing));}
    void Validate()
    {
        const auto world=[&](glm::vec3 p){return(p-origin)*scale;};
        if(At(world({-114,2,330}))!=Zone::Silent||At(world({-114.75f,2,150}))!=Zone::Corridor||
            At(world({lakeMin.x-3,2,-29}))!=Zone::Lagoon||At(glm::vec3(0),true)!=Zone::Lagoon)
            throw std::runtime_error("Clasificacion de audio por zonas incorrecta.");
        for(auto zone:{Zone::Corridor,Zone::Lagoon,Zone::Silent})
        {
            Switch(zone);
            const bool a=Playing(L"festival_corridor"),b=Playing(L"festival_lagoon");
            if(a&&b)throw std::runtime_error("Las pistas ambientales se empalman.");
            if((zone==Zone::Corridor&&corridorOpen&&!a)||(zone==Zone::Lagoon&&lagoonOpen&&!b)||
                (zone==Zone::Silent&&(a||b)))throw std::runtime_error("Transicion de audio incorrecta.");
        }
        std::cout<<"Audio OK: silencio Zocalo, Pasillo repeat, Laguna/bote repeat; transiciones sin empalmes."<<std::endl;
    }
private:
    Zone current=Zone::Silent;int volume=380;bool corridorOpen=false,lagoonOpen=false,configured=false;
    glm::vec3 origin{},plazaLo{},plazaHi{},corridorLo{},corridorHi{},lakeMin{},lakeMax{};float scale=1;
    static MCIERROR Send(const std::wstring& command){return mciSendStringW(command.c_str(),nullptr,0,nullptr);}
    bool Open(const wchar_t* file,const wchar_t* alias)
    {
        const auto absolute=std::filesystem::absolute(file);
        if(!std::filesystem::is_regular_file(absolute))throw std::runtime_error("Falta un MP3 de audio ambiental.");
        const auto error=Send(L"open \""+absolute.wstring()+L"\" type mpegvideo alias "+alias);
        if(error){std::cerr<<"Audio: dispositivo MP3 no disponible ("<<error<<")."<<std::endl;return false;}
        if(Send(std::wstring(L"setaudio ")+alias+L" volume to "+std::to_wstring(volume)))
        {Send(std::wstring(L"close ")+alias);std::cerr<<"Audio: no se pudo fijar el volumen moderado."<<std::endl;return false;}
        return true;
    }
    static bool Playing(const wchar_t* alias)
    {
        wchar_t mode[64]{};const auto command=std::wstring(L"status ")+alias+L" mode";
        return !mciSendStringW(command.c_str(),mode,64,nullptr)&&std::wstring(mode)==L"playing";
    }
    void StopBoth(){if(corridorOpen)Send(L"stop festival_corridor");if(lagoonOpen)Send(L"stop festival_lagoon");}
    void Switch(Zone zone)
    {
        if(zone==current)return;
        StopBoth();current=zone;
        const wchar_t* alias=zone==Zone::Corridor&&corridorOpen?L"festival_corridor":zone==Zone::Lagoon&&lagoonOpen?L"festival_lagoon":nullptr;
        if(alias){Send(std::wstring(L"seek ")+alias+L" to start");if(Send(std::wstring(L"play ")+alias+L" repeat"))current=Zone::Silent;}
    }
};
