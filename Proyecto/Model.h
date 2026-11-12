#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include "Mesh.h"
#include "UnicodeIO.h"
#include "GroundSurface.h"
#include "SkeletalAnimator.h"
#include "Texture2D.h"
#include "FloorUV.h"
#include "MovementSettings.h"
#include "DockSurface.h"

struct ModelOptions
{
    GroundSurface* ground = nullptr;
    bool skeletal = false;
    std::vector<std::string> boundaryProfileMaterials;
    bool requireLocomotion=true;
};

class Model
{
public:
    explicit Model(const char* path, const std::function<bool()>& keepLoading = {},
        ModelOptions options = {}) : options(options)
    {
        try
        {
            const std::string assetPath(path);
            if(assetPath.size()>=6&&assetPath.compare(assetPath.size()-6,6,".xanim")==0)
                LoadVertexAnimation(assetPath);
            else LoadModel(assetPath, keepLoading);
        }
        catch (...) { ReleaseTextures(); throw; }
    }
    ~Model() { if(instanceBuffer)glDeleteBuffers(1,&instanceBuffer);ReleaseTextures(); }
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    void Draw(const Shader& shader,const glm::mat4& clipFromModel,bool cull,const glm::mat4& baseModel) const
    {
        const auto basePlanes=ExtractFrustum(clipFromModel);
        if(cull&&!BoundsVisible(basePlanes,minBounds,maxBounds)){++RenderMetrics::culled;return;}
        const auto baseNormal=glm::transpose(glm::inverse(glm::mat3(baseModel)));
        glm::mat4 previousLocal(0);bool matricesSet=false;
        for (const Mesh& mesh : meshes)
        {
            const auto planes=mesh.HasIdentityTransform()?basePlanes:ExtractFrustum(clipFromModel*mesh.LocalTransform());
            if(cull&&!mesh.IsVisible(planes)){++RenderMetrics::culled;continue;}
            if(!matricesSet||previousLocal!=mesh.LocalTransform())
            {
                const auto model=baseModel*mesh.LocalTransform();const auto normal=baseNormal*mesh.LocalNormal();
                glUniformMatrix4fv(shader.Uniform("model"),1,GL_FALSE,&model[0][0]);
                glUniformMatrix3fv(shader.Uniform("normalMatrix"),1,GL_FALSE,&normal[0][0]);
                previousLocal=mesh.LocalTransform();matricesSet=true;
            }
            mesh.Draw(shader,cull?&planes:nullptr);
        }
    }
    void DrawInstances(const Shader& shader,const std::vector<glm::mat4>& transforms,const glm::mat4& viewProjection) const
    {
        if(options.skeletal)throw std::runtime_error("El lote estatico no acepta modelos con huesos.");
        const auto planes=ExtractFrustum(viewProjection);visibleInstances.clear();
        for(const auto& transform:transforms)
            if(TransformedBoundsVisible(planes,minBounds,maxBounds,transform))visibleInstances.push_back(transform);
            else ++RenderMetrics::culled;
        if(visibleInstances.empty())return;
        shader.Use();glUniformMatrix4fv(shader.Uniform("viewProjection"),1,GL_FALSE,&viewProjection[0][0]);
        if(!instanceBuffer)glGenBuffers(1,&instanceBuffer);
        if(uploadedInstances!=visibleInstances)
        {
            glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer);
            glBufferData(GL_ARRAY_BUFFER,visibleInstances.size()*sizeof(glm::mat4),visibleInstances.data(),GL_STREAM_DRAW);
            uploadedInstances=visibleInstances;
        }
        for(const auto& mesh:meshes)
        {
            if(!mesh.HasIdentityTransform())throw std::runtime_error("El lote de farolas requiere mallas en el espacio comun del modelo.");
            mesh.DrawInstanced(shader,instanceBuffer,static_cast<GLsizei>(visibleInstances.size()));
        }
    }
    const glm::vec3& GetMinBounds() const { return minBounds; }
    const glm::vec3& GetMaxBounds() const { return maxBounds; }
    SkeletalAnimator* Animator() { return skeleton.get(); }
    size_t EmbeddedTextureCount() const{return embeddedTextureCount;}
    struct DoorBox{glm::vec3 min,max;};
    struct DoorOpening{glm::vec3 min,max;int normalAxis;};
    const std::vector<DoorOpening>& DoorOpenings() const{return doorOpenings;}
    const std::vector<DoorBox>& DoorBoxes() const{return doorBoxes;}
    struct BenchSeat{glm::vec3 anchor;float yaw;float floor;};
    const std::vector<BenchSeat>& BenchSeats() const{return benchSeats;}
    void ValidateLayout() const
    {
        if(!options.ground)return;
        glm::vec3 lo,hi;
        if(!GetMaterialBounds("Material.014",lo,hi)||benchCount!=8||benchMaxHeight>2.1f||
            std::abs((hi.y-lo.y)/kioskRawHeight-MovementSettings::KioskScale)>.001f)
            throw std::runtime_error("Las proporciones del kiosko o de las ocho bancas no son correctas.");
        glm::vec3 fountainLo,fountainHi;
        if(!GetMaterialBounds("Material.004",fountainLo,fountainHi)||
            std::abs((fountainHi.x-fountainLo.x)/fountainRawWidth-MovementSettings::FountainScale)>.001f||
            std::max(fountainHi.x-fountainLo.x,fountainHi.z-fountainLo.z)>(hi.x-lo.x)*.4f)
            throw std::runtime_error("La fuente no tiene escala independiente o invade los pilares.");
        std::cout<<"Proporciones OK: kiosko x"<<MovementSettings::KioskScale<<", "<<benchCount<<" bancas x"<<MovementSettings::BenchScale<<", respaldo maximo "<<benchMaxHeight<<"."<<std::endl;
        std::cout<<"Fuente independiente x"<<MovementSettings::FountainScale<<", ancho "<<fountainHi.x-fountainLo.x<<"; kiosko sin cambios."<<std::endl;
        glm::vec3 parkLo,parkHi;
        if(!GetMaterialBounds("Material.006",parkLo,parkHi))throw std::runtime_error("Falta piso interior.");
        for(const auto& b:doorBoxes)
            if(b.min.x<parkLo.x||b.max.x>parkHi.x||b.min.z<parkLo.z||b.max.z>parkHi.z||std::abs(b.min.y-parkHi.y)>.01f)
                throw std::runtime_error("Un arco no queda estrictamente dentro y al ras del piso del parque.");
        if(dockTriangles<8||dockUvSpan.x<20||dockUvSpan.y<10)throw std::runtime_error("El muelle no tiene geometria/UV repetidas correctas.");
        std::cout<<"Muelle OK: "<<dockTriangles<<" triangulos alineados; UV repetidas "<<dockUvSpan.x<<" x "<<dockUvSpan.y<<"."<<std::endl;
    }
    bool GetMaterialBounds(const std::string& name, glm::vec3& min, glm::vec3& max) const
    {
        const auto found = materialBounds.find(name);
        if (found == materialBounds.end()) return false;
        min = found->second.min; max = found->second.max;
        return true;
    }
    const std::vector<glm::vec3>* GetMaterialPositions(const std::string& name) const
    {
        const auto found = materialProfiles.find(name);
        return found == materialProfiles.end() ? nullptr : &found->second;
    }
    bool HasBakedAnimation() const{return !bakedMeshes.empty();}
    void UpdateBakedAnimation(float time)
    {
        if(bakedMeshes.empty())return;
        const float wrapped=std::fmod(std::max(0.0f,time),bakedDuration);
        const float sample=wrapped/bakedDuration*static_cast<float>(bakedMeshes.front().frames.size()-1);
        const size_t first=static_cast<size_t>(sample),second=std::min(first+1,bakedMeshes.front().frames.size()-1);
        const float blend=sample-static_cast<float>(first);
        for(size_t m=0;m<bakedMeshes.size();++m)
        {
            auto& vertices=bakedMeshes[m].scratch;
            if(vertices.empty())vertices=bakedMeshes[m].base;
            const auto& a=bakedMeshes[m].frames[first];const auto& b=bakedMeshes[m].frames[second];
            for(size_t v=0;v<vertices.size();++v)
            {
                vertices[v].Position=glm::mix(a[v].position,b[v].position,blend);
                const auto normal=glm::mix(a[v].normal,b[v].normal,blend);
                vertices[v].Normal=glm::length(normal)>0.00001f?glm::normalize(normal):a[v].normal;
            }
            meshes[m].UpdateVertices(vertices);
        }
    }
    bool GetMaterialEndBounds(const std::string& name,int axis,float band,glm::vec3& min,glm::vec3& max) const
    {
        glm::vec3 lo,hi;
        const auto profile=materialProfiles.find(name);
        if(axis<0||axis>2||profile==materialProfiles.end()||!GetMaterialBounds(name,lo,hi)) return false;
        min=glm::vec3((std::numeric_limits<float>::max)());
        max=glm::vec3((std::numeric_limits<float>::lowest)());bool found=false;
        for(const auto& p:profile->second) if(p[axis]>=hi[axis]-band)
        {min=glm::min(min,p);max=glm::max(max,p);found=true;}
        return found;
    }
    bool GetMaterialSectionBounds(const std::string& name,int axis,float from,float to,glm::vec3& min,glm::vec3& max) const
    {
        const auto profile=materialProfiles.find(name);
        if(profile==materialProfiles.end()||axis<0||axis>2||from>to)return false;
        min=glm::vec3((std::numeric_limits<float>::max)());max=glm::vec3((std::numeric_limits<float>::lowest)());bool found=false;
        for(const auto& p:profile->second)if(p[axis]>=from&&p[axis]<=to)
        {min=glm::min(min,p);max=glm::max(max,p);found=true;}
        return found;
    }

private:
    class ImportProgress : public Assimp::ProgressHandler
    {
    public:
        explicit ImportProgress(const std::atomic<bool>& cancelled) : cancelled(cancelled) {}
        bool Update(float = -1.0f) override { return !cancelled.load(); }
    private:
        const std::atomic<bool>& cancelled;
    };

    std::vector<Mesh> meshes;
    mutable GLuint instanceBuffer=0;
    mutable std::vector<glm::mat4> visibleInstances,uploadedInstances;
    std::unordered_map<std::string, GLuint> texturesLoaded;
    std::string directory;
    ModelOptions options;
    std::unordered_map<std::string,std::vector<glm::vec3>> materialProfiles;
    std::unique_ptr<SkeletalAnimator> skeleton;
    struct BakedVertex {glm::vec3 position,normal;};
    struct BakedMeshFrames {std::vector<Vertex> base,scratch;std::vector<std::vector<BakedVertex>> frames;};
    std::vector<BakedMeshFrames> bakedMeshes;
    float bakedDuration=0.0f;
    struct Bounds { glm::vec3 min, max; };
    std::unordered_map<std::string, Bounds> materialBounds;
    glm::mat4 kioskTransform=glm::mat4(1);
    glm::mat4 fountainTransform=glm::mat4(1);
    float floorLevel=2.005836f,kioskRawHeight=1,fountainRawWidth=1,benchMaxHeight=0;
    size_t benchCount=0;
    std::vector<BenchSeat> benchSeats;
    std::vector<DoorBox> doorBoxes;
    std::vector<DoorOpening> doorOpenings;
    size_t embeddedTextureCount=0;
    glm::vec3 plazaCenter=glm::vec3(0);
    GroundSurface doorGround;
    size_t dockTriangles=0;glm::vec2 dockUvSpan=glm::vec2(0);
    glm::vec3 minBounds = glm::vec3((std::numeric_limits<float>::max)());
    glm::vec3 maxBounds = glm::vec3((std::numeric_limits<float>::lowest)());

    void ReleaseTextures()
    {
        for (const auto& texture : texturesLoaded) glDeleteTextures(1, &texture.second);
        texturesLoaded.clear();
    }
    void LoadModel(const std::string& path, const std::function<bool()>& keepLoading)
    {
        std::cout << "Cargando " << path << " (puede tardar unos segundos)..." << std::endl;
        std::atomic<bool> cancelled(false);
        Assimp::Importer importer;
        importer.SetIOHandler(new UnicodeIO());
        importer.SetProgressHandler(new ImportProgress(cancelled));
        // Escenario estatico: aplicar nodos de Blender, normales faltantes y UV del MTL (-s).
        const unsigned int flags = aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
            aiProcess_TransformUVCoords | aiProcess_FlipUVs |
            (options.skeletal ? aiProcess_LimitBoneWeights : aiProcess_PreTransformVertices);
        if (options.skeletal) importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
        // Solo Assimp trabaja en segundo plano. Eventos y recursos OpenGL permanecen
        // en el hilo que posee el contexto durante toda la importacion.
        auto importedScene = std::async(std::launch::async, [&importer, &path, flags]() {
            return importer.ReadFile(path, flags);
        });
        while (importedScene.wait_for(std::chrono::milliseconds(16)) != std::future_status::ready)
            if (keepLoading && !keepLoading()) cancelled.store(true);
        const aiScene* scene = importedScene.get();
        if (cancelled.load()) throw std::runtime_error("Carga del escenario cancelada.");
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
            throw std::runtime_error("ASSIMP: " + std::string(importer.GetErrorString()));
        const size_t separator = path.find_last_of("/\\");
        directory = separator == std::string::npos ? "." : path.substr(0, separator);
        if (options.skeletal)
        {
            skeleton = std::make_unique<SkeletalAnimator>(scene,options.requireLocomotion);
            std::cout << "Esqueleto real: " << skeleton->BoneCount() << " huesos deformantes, "
                << skeleton->ClipCount() << " clips." << std::endl;
        }
        if(options.ground)ConfigureLayout(scene);
        meshes.reserve(scene->mNumMeshes);
        ProcessNode(scene->mRootNode, scene);
        if (meshes.empty()) throw std::runtime_error("El escenario no contiene triangulos.");
        std::cout << "Entorno listo: " << meshes.size() << " mallas, " << texturesLoaded.size()
            << " texturas unicas." << std::endl;
        // El importer libera su copia de la geometria antes de comenzar el render loop.
    }
    void ProcessNode(const aiNode* node, const aiScene* scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
            ProcessMesh(scene->mMeshes[node->mMeshes[i]], scene);
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            ProcessNode(node->mChildren[i], scene);
    }
    void ConfigureLayout(const aiScene* scene)
    {
        glm::vec3 lo((std::numeric_limits<float>::max)()),hi((std::numeric_limits<float>::lowest)());bool found=false;
        glm::vec3 fountainLo=lo,fountainHi=hi;bool foundFountain=false;
        for(unsigned m=0;m<scene->mNumMeshes;++m)
        {
            const auto* mesh=scene->mMeshes[m];aiString name;scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME,name);
            const std::string material=name.C_Str();
            if(material!="Material.014"&&material!="Material.006"&&material!="Material.004")continue;
            for(unsigned v=0;v<mesh->mNumVertices;++v)
            {
                const auto& p=mesh->mVertices[v];
                if(material=="Material.006")floorLevel=std::max(floorLevel,p.y);
                else if(material=="Material.004")
                {fountainLo=glm::min(fountainLo,glm::vec3(p.x,p.y,p.z));fountainHi=glm::max(fountainHi,glm::vec3(p.x,p.y,p.z));foundFountain=true;}
                else{lo=glm::min(lo,glm::vec3(p.x,p.y,p.z));hi=glm::max(hi,glm::vec3(p.x,p.y,p.z));found=true;}
            }
        }
        if(!found||!foundFountain)throw std::runtime_error("No se pudo separar el kiosko y su fuente del entorno.");
        plazaCenter=(lo+hi)*.5f;
        for(unsigned m=0;m<scene->mNumMeshes;++m)
        {
            const auto* mesh=scene->mMeshes[m];aiString name;scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME,name);
            const std::string n=name.C_Str();
            if(n!="Material.006"&&n!="Material.013"&&n!="Material.016"&&n!="Material.017"&&n!="Material.018"&&n!="Material.019"&&n!="Material.020")continue;
            for(unsigned f=0;f<mesh->mNumFaces;++f)if(mesh->mFaces[f].mNumIndices==3)
            {
                glm::vec3 p[3];for(int i=0;i<3;++i){const auto& v=mesh->mVertices[mesh->mFaces[f].mIndices[i]];p[i]={v.x,v.y,v.z};}
                doorGround.Add(p[0],p[1],p[2]);
            }
        }
        const glm::vec3 anchor((lo.x+hi.x)*.5f,floorLevel,(lo.z+hi.z)*.5f);
        kioskTransform=glm::translate(glm::mat4(1),anchor);
        kioskTransform=glm::scale(kioskTransform,glm::vec3(MovementSettings::KioskScale));
        kioskTransform=glm::translate(kioskTransform,-anchor);
        kioskRawHeight=hi.y-lo.y;
        const glm::vec3 fountainAnchor((fountainLo.x+fountainHi.x)*.5f,floorLevel,(fountainLo.z+fountainHi.z)*.5f);
        fountainTransform=glm::translate(glm::mat4(1),fountainAnchor);
        fountainTransform=glm::scale(fountainTransform,glm::vec3(MovementSettings::FountainScale));
        fountainTransform=glm::translate(fountainTransform,-fountainAnchor);
        fountainRawWidth=fountainHi.x-fountainLo.x;
    }
    void AddPart(const std::vector<Vertex>& vertices,const std::vector<GLuint>& indices,const std::vector<SkinVertex>& skin,
        const MeshMaterial& material,const std::string& name,const glm::mat4& transform)
    {
        glm::vec3 lo((std::numeric_limits<float>::max)()),hi((std::numeric_limits<float>::lowest)()),worldLo=lo,worldHi=hi;
        for(const auto& v:vertices)
        {
            lo=glm::min(lo,v.Position);hi=glm::max(hi,v.Position);
            const auto p=glm::vec3(transform*glm::vec4(v.Position,1));worldLo=glm::min(worldLo,p);worldHi=glm::max(worldHi,p);
        }
        if(!options.skeletal){minBounds=glm::min(minBounds,worldLo);maxBounds=glm::max(maxBounds,worldHi);}
        auto found=materialBounds.find(name);
        if(found==materialBounds.end())materialBounds.emplace(name,Bounds{worldLo,worldHi});
        else{found->second.min=glm::min(found->second.min,worldLo);found->second.max=glm::max(found->second.max,worldHi);}
        if(options.ground&&GroundSurface::IsWalkableMaterial(name))
            for(size_t i=0;i+2<indices.size();i+=3)
                options.ground->Add(glm::vec3(transform*glm::vec4(vertices[indices[i]].Position,1)),
                    glm::vec3(transform*glm::vec4(vertices[indices[i+1]].Position,1)),
                    glm::vec3(transform*glm::vec4(vertices[indices[i+2]].Position,1)),GroundSurface::IsRaisedGrass(name));
        meshes.emplace_back(vertices,indices,material,lo,hi,skin,transform,options.ground!=nullptr);
    }
    void AddBenches(const std::vector<Vertex>& vertices,const std::vector<GLuint>& indices,const MeshMaterial& material,const std::string& name)
    {
        constexpr size_t count=8;std::array<glm::vec2,count> centers{};
        centers[0]={vertices.front().Position.x,vertices.front().Position.z};
        for(size_t seed=1;seed<count;++seed)
        {
            float farthest=-1;
            for(const auto& vertex:vertices)
            {
                const glm::vec2 p(vertex.Position.x,vertex.Position.z);float nearest=(std::numeric_limits<float>::max)();
                for(size_t i=0;i<seed;++i)nearest=std::min(nearest,glm::dot(p-centers[i],p-centers[i]));
                if(nearest>farthest){farthest=nearest;centers[seed]=p;}
            }
        }
        const auto nearest=[&](const glm::vec2& p){size_t best=0;float distance=(std::numeric_limits<float>::max)();
            for(size_t i=0;i<count;++i){float d=glm::dot(p-centers[i],p-centers[i]);if(d<distance){distance=d;best=i;}}return best;};
        for(int iteration=0;iteration<40;++iteration)
        {
            std::array<glm::vec2,count> sum;sum.fill(glm::vec2(0));std::array<unsigned,count> counts{};
            for(const auto& vertex:vertices){const glm::vec2 p(vertex.Position.x,vertex.Position.z);const auto id=nearest(p);sum[id]+=p;++counts[id];}
            for(size_t i=0;i<count;++i)if(counts[i])centers[i]=sum[i]/static_cast<float>(counts[i]);
        }
        std::array<std::vector<GLuint>,count> groups;
        for(size_t i=0;i+2<indices.size();i+=3)
        {
            const auto p=(vertices[indices[i]].Position+vertices[indices[i+1]].Position+vertices[indices[i+2]].Position)/3.0f;
            auto& group=groups[nearest({p.x,p.z})];group.insert(group.end(),indices.begin()+i,indices.begin()+i+3);
        }
        for(const auto& group:groups)
        {
            if(group.empty())throw std::runtime_error("No se pudieron aislar las ocho bancas.");
            std::vector<GLuint> remap(vertices.size(),(std::numeric_limits<GLuint>::max)()),localIndices;
            std::vector<Vertex> localVertices;glm::vec3 lo((std::numeric_limits<float>::max)()),hi((std::numeric_limits<float>::lowest)());
            for(GLuint old:group)
            {
                if(remap[old]==(std::numeric_limits<GLuint>::max)())
                {remap[old]=static_cast<GLuint>(localVertices.size());localVertices.push_back(vertices[old]);lo=glm::min(lo,vertices[old].Position);hi=glm::max(hi,vertices[old].Position);}
                localIndices.push_back(remap[old]);
            }
            if(std::max(hi.x-lo.x,hi.z-lo.z)>16)throw std::runtime_error("Una agrupacion de bancas contiene mas de un asiento.");
            const glm::vec3 sourceAnchor((lo.x+hi.x)*.5f,lo.y,(lo.z+hi.z)*.5f),targetAnchor(sourceAnchor.x,floorLevel,sourceAnchor.z);
            auto transform=glm::translate(glm::mat4(1),targetAnchor);
            transform=glm::scale(transform,glm::vec3(MovementSettings::BenchScale));transform=glm::translate(transform,-sourceAnchor);
            // Detectar el asiento por AREA horizontal, y el respaldo por sus
            // vertices altos. No usar el centro de toda la banca para sentar.
            struct SeatBin{double area=0;glm::vec3 weighted=glm::vec3(0);};std::map<int,SeatBin> seatBins;
            for(size_t i=0;i+2<localIndices.size();i+=3)
            {
                const auto a=localVertices[localIndices[i]].Position,b=localVertices[localIndices[i+1]].Position,c=localVertices[localIndices[i+2]].Position;
                const auto n=glm::cross(b-a,c-a);const float area=glm::length(n),y=(a.y+b.y+c.y)/3;
                if(area>1e-6f&&std::abs(n.y)>.95f*area&&y>lo.y+(hi.y-lo.y)*.22f&&y<lo.y+(hi.y-lo.y)*.72f)
                {auto& bin=seatBins[static_cast<int>(std::round(y*50))];bin.area+=area;bin.weighted+=(a+b+c)*(area/3);}
            }
            if(seatBins.empty())throw std::runtime_error("No se pudo medir el asiento de una banca.");
            const auto seatIt=std::max_element(seatBins.begin(),seatBins.end(),[](const auto& a,const auto& b){return a.second.area<b.second.area;});
            const auto seat=seatIt->second.weighted/static_cast<float>(seatIt->second.area);
            glm::vec3 back(0);unsigned backCount=0;
            for(const auto& v:localVertices)if(v.Position.y>lo.y+(hi.y-lo.y)*.80f){back+=v.Position;++backCount;}
            back/=static_cast<float>(std::max(1u,backCount));glm::vec3 front=seat-back;front.y=0;
            if(glm::length(front)<.02f)front=glm::vec3(-114-seat.x,0,331.5f-seat.z);
            front=glm::normalize(front);
            benchSeats.push_back({glm::vec3(transform*glm::vec4(seat,1)),std::atan2(front.x,front.z),floorLevel});
            benchMaxHeight=std::max(benchMaxHeight,(hi.y-lo.y)*MovementSettings::BenchScale);++benchCount;
            AddPart(localVertices,localIndices,{},material,name,transform);
        }
    }
    void ProcessMesh(const aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<SkinVertex> skin;
        std::string materialName;
        if (mesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiString name;
            scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, name);
            materialName = name.C_Str();
        }
        if (options.skeletal) skin = skeleton->Weights(mesh);
        glm::vec3 meshMin((std::numeric_limits<float>::max)());
        glm::vec3 meshMax((std::numeric_limits<float>::lowest)());
        vertices.reserve(mesh->mNumVertices);
        indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            Vertex vertex{};
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            if(options.ground&&materialName=="tarima_madera.001")vertex.Position=DockSurface::AlignVertex(vertex.Position);
            vertex.Normal = mesh->HasNormals()
                ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            if (mesh->HasTextureCoords(0))
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            const glm::vec3 boundPosition = options.skeletal
                ? skeleton->BindPosition(vertex.Position, skin[i]) : vertex.Position;
            if(options.skeletal){minBounds=glm::min(minBounds,boundPosition);maxBounds=glm::max(maxBounds,boundPosition);}
            meshMin = glm::min(meshMin, boundPosition);
            meshMax = glm::max(meshMax, boundPosition);
            vertices.push_back(vertex);
        }
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            if (face.mNumIndices == 3) indices.insert(indices.end(), face.mIndices, face.mIndices + 3);
        }
        if (indices.empty()) return;
        // Solo perfiles pequenos: tapete terminal y tablones del muelle.
        if(std::find(options.boundaryProfileMaterials.begin(),options.boundaryProfileMaterials.end(),materialName)!=options.boundaryProfileMaterials.end())
            for(const auto& vertex:vertices)materialProfiles[materialName].push_back(vertex.Position);
        MeshMaterial material;
        if (mesh->mMaterialIndex < scene->mNumMaterials)
        {
            const aiMaterial* imported = scene->mMaterials[mesh->mMaterialIndex];
            aiColor3D color(1.0f, 1.0f, 1.0f);
            if (imported->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
                material.diffuseColor = glm::vec3(color.r, color.g, color.b);
            if (imported->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
                material.emissiveColor = glm::vec3(color.r, color.g, color.b);
            imported->Get(AI_MATKEY_OPACITY, material.opacity);
            aiColor4D baseColor;
            const bool hasBaseColor=imported->Get("$clr.base",0,0,baseColor)==AI_SUCCESS||
                imported->Get("$mat.gltf.pbrMetallicRoughness.baseColorFactor",0,0,baseColor)==AI_SUCCESS;
            if(hasBaseColor){material.diffuseColor={baseColor.r,baseColor.g,baseColor.b};material.opacity*=baseColor.a;}
            const auto albedoType=imported->GetTextureCount(aiTextureType_DIFFUSE)>0?aiTextureType_DIFFUSE:aiTextureType_UNKNOWN;
            if (imported->GetTextureCount(albedoType) > 0)
            {
                aiString texturePath;
                if (imported->GetTexture(albedoType, 0, &texturePath) == AI_SUCCESS)
                {
                    material.diffuseTexture = LoadTexture(texturePath.C_Str(),scene);
                    // Blender omite Kd cuando el color procede de una imagen.
                    if(!hasBaseColor)material.diffuseColor = glm::vec3(1.0f);
                }
            }
        }
        const glm::vec2 tile = options.ground ? ZocaloTileSize(materialName) : glm::vec2(0);
        if (tile.x > 0.0f)
            for (auto& vertex : vertices) vertex.TexCoords = ProjectFloorUV(vertex.Position, vertex.Normal, tile);
        if(options.ground&&materialName=="tarima_madera.001")AddDock(vertices,indices,material,materialName);
        else if(options.ground&&materialName=="LoD_0SG")AddBenches(vertices,indices,material,materialName);
        else if(options.ground&&materialName=="Stuco_Wall")AddDoors(vertices,indices,material,materialName);
        else
        {
            const bool kiosk=options.ground&&materialName=="Material.014";
            const bool fountain=options.ground&&(materialName=="Material.004"||materialName=="Material.005"||materialName=="Material.007");
            AddPart(vertices,indices,skin,material,materialName,kiosk?kioskTransform:fountain?fountainTransform:glm::mat4(1));
        }
    }
    void AddDoors(const std::vector<Vertex>& vertices,const std::vector<GLuint>& indices,const MeshMaterial& material,const std::string& name)
    {
        // Cuatro portadas cardinales; el adorno central del kiosko se conserva.
        std::array<std::vector<GLuint>,5> groups;
        for(size_t i=0;i+2<indices.size();i+=3)
        {
            const auto p=(vertices[indices[i]].Position+vertices[indices[i+1]].Position+vertices[indices[i+2]].Position)/3.0f;
            const auto d=p-plazaCenter;int group=4;
            if(std::max(std::abs(d.x),std::abs(d.z))>30)
                group=std::abs(d.x)>std::abs(d.z)?(d.x>0?0:1):(d.z>0?2:3);
            groups[group].insert(groups[group].end(),indices.begin()+i,indices.begin()+i+3);
        }
        for(size_t group=0;group<groups.size();++group)
        {
            if(groups[group].empty()){if(group<4)throw std::runtime_error("Falta una puerta blanca.");continue;}
            std::vector<Vertex> local;std::vector<GLuint> localIndices;
            std::unordered_map<GLuint,GLuint> remap;
            glm::vec3 lo((std::numeric_limits<float>::max)()),hi((std::numeric_limits<float>::lowest)());
            for(GLuint id:groups[group])
            {
                auto found=remap.find(id);
                if(found==remap.end())
                {const auto index=static_cast<GLuint>(local.size());remap.emplace(id,index);local.push_back(vertices[id]);localIndices.push_back(index);
                 lo=glm::min(lo,vertices[id].Position);hi=glm::max(hi,vertices[id].Position);}
                else localIndices.push_back(found->second);
            }
            glm::vec3 offset(0);
            if(group<4)
            {
                // Dentro del piso del parque: tres unidades hacia el kiosko.
                offset=group==0?glm::vec3(-3,0,0):group==1?glm::vec3(3,0,0):group==2?glm::vec3(0,0,-3):glm::vec3(0,0,3);
                const auto center=(lo+hi)*.5f+offset;float support=floorLevel;
                if(!doorGround.Sample(center.x,center.z,floorLevel,support))throw std::runtime_error("Puerta sin piso de apoyo.");
                offset.y=support-lo.y;
                const int span=group<2?2:0,normalAxis=group<2?0:2;
                const float middle=(lo[span]+hi[span])*.5f;
                glm::vec3 pillarLo[2]={glm::vec3((std::numeric_limits<float>::max)()),glm::vec3((std::numeric_limits<float>::max)())};
                glm::vec3 pillarHi[2]={glm::vec3((std::numeric_limits<float>::lowest)()),glm::vec3((std::numeric_limits<float>::lowest)())};
                // Solo el tramo vertical inferior. La boveda/lintel no cierra el hueco.
                for(const auto& v:local)if(v.Position.y<=lo.y+8.0f)
                {const int side=v.Position[span]<middle?0:1;pillarLo[side]=glm::min(pillarLo[side],v.Position);pillarHi[side]=glm::max(pillarHi[side],v.Position);}
                for(int side=0;side<2;++side)
                {
                    pillarLo[side].y=lo.y;pillarHi[side].y=lo.y+8.0f;
                    if(pillarHi[side][span]<=pillarLo[side][span])throw std::runtime_error("Pilar vacio.");
                    doorBoxes.push_back({pillarLo[side]+offset,pillarHi[side]+offset});
                }
                auto openingLo=lo+offset,openingHi=hi+offset;
                openingLo[span]=pillarHi[0][span]+offset[span];openingHi[span]=pillarLo[1][span]+offset[span];openingHi.y=openingLo.y+8.0f;
                doorOpenings.push_back({openingLo,openingHi,normalAxis});
                std::cout<<"Puerta "<<group+1<<": traslacion "<<offset.x<<", "<<offset.y<<", "<<offset.z<<"; dos AABB de pilares, centro libre."<<std::endl;
            }
            AddPart(local,localIndices,{},material,name,glm::translate(glm::mat4(1),offset));
        }
        if(doorBoxes.size()!=8||doorOpenings.size()!=4)throw std::runtime_error("Se requieren ocho pilares y cuatro pasos libres.");
    }
    void AddDock(const std::vector<Vertex>& vertices,const std::vector<GLuint>& indices,const MeshMaterial& original,const std::string& name)
    {
        std::vector<GLuint> westIndices,eastIndices;std::vector<Vertex> east;
        glm::vec2 uvLo((std::numeric_limits<float>::max)()),uvHi((std::numeric_limits<float>::lowest)());
        for(size_t i=0;i+2<indices.size();i+=3)
        {
            if(!DockSurface::IsDock(vertices[indices[i]].Position))
            {westIndices.insert(westIndices.end(),indices.begin()+i,indices.begin()+i+3);continue;}
            const auto a=vertices[indices[i]].Position,b=vertices[indices[i+1]].Position,c=vertices[indices[i+2]].Position;
            const auto cross=glm::cross(b-a,c-a);if(glm::length(cross)<1e-6f)continue;
            const auto normal=glm::normalize(cross);
            for(int j=0;j<3;++j)
            {
                auto v=vertices[indices[i+j]];v.Normal=normal;v.TexCoords=DockSurface::UV(v.Position,normal);
                uvLo=glm::min(uvLo,v.TexCoords);uvHi=glm::max(uvHi,v.TexCoords);
                eastIndices.push_back(static_cast<GLuint>(east.size()));east.push_back(v);
            }
            ++dockTriangles;
        }
        if(!westIndices.empty())AddPart(vertices,westIndices,{},original,name,glm::mat4(1));
        if(east.empty())throw std::runtime_error("No se separo el muelle positivo de la tarima occidental.");
        MeshMaterial wood=original;wood.diffuseTexture=LoadTexture("../../images/MuelleMadera.png");wood.diffuseColor=glm::vec3(1);
        dockUvSpan=glm::max(dockUvSpan,uvHi-uvLo);AddPart(east,eastIndices,{},wood,name,glm::mat4(1));
    }
    GLuint LoadTexture(std::string relativePath,const aiScene* scene=nullptr)
    {
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
        if(!relativePath.empty()&&relativePath[0]=='*')
        {
            const auto existing=texturesLoaded.find(relativePath);if(existing!=texturesLoaded.end())return existing->second;
            size_t consumed=0;unsigned long index=0;
            try{index=std::stoul(relativePath.substr(1),&consumed);}catch(...){throw std::runtime_error("Indice de textura incrustada invalido.");}
            if(!scene||consumed!=relativePath.size()-1||index>=scene->mNumTextures)throw std::runtime_error("Falta imagen incrustada: "+relativePath);
            const auto* image=scene->mTextures[index];GLuint texture=0;
            if(image->mHeight==0)texture=DecodeRgbaTexture(reinterpret_cast<const unsigned char*>(image->pcData),image->mWidth);
            else
            {
                if(image->mWidth>static_cast<unsigned>((std::numeric_limits<int>::max)())||image->mHeight>static_cast<unsigned>((std::numeric_limits<int>::max)()))throw std::runtime_error("Imagen incrustada demasiado grande.");
                std::vector<unsigned char> rgba(static_cast<size_t>(image->mWidth)*image->mHeight*4);
                for(size_t i=0;i<rgba.size()/4;++i){const auto& p=image->pcData[i];rgba[i*4]=p.r;rgba[i*4+1]=p.g;rgba[i*4+2]=p.b;rgba[i*4+3]=p.a;}
                texture=UploadRgbaTexture(rgba.data(),static_cast<int>(image->mWidth),static_cast<int>(image->mHeight));
            }
            texturesLoaded.emplace(relativePath,texture);++embeddedTextureCount;
            std::cout<<"Textura incrustada "<<relativePath<<" cargada en GPU."<<std::endl;return texture;
        }
        const bool absolute = (!relativePath.empty() && relativePath[0] == '/') ||
            (relativePath.size() > 1 && relativePath[1] == ':');
        const std::string filename = absolute ? relativePath : directory + '/' + relativePath;
        const auto existing = texturesLoaded.find(filename);
        if (existing != texturesLoaded.end()) return existing->second;
        const GLuint texture = LoadRgbaTexture(filename);
        texturesLoaded.emplace(filename, texture);
        return texture;
    }
    template<typename T> static T ReadBinaryValue(std::istream& input)
    {
        T value{};input.read(reinterpret_cast<char*>(&value),sizeof(T));
        if(!input)throw std::runtime_error("Archivo de animacion de vertices truncado.");
        return value;
    }
    static std::string ReadBinaryString(std::istream& input)
    {
        const uint32_t length=ReadBinaryValue<uint32_t>(input);
        if(length>4096)throw std::runtime_error("Cadena demasiado larga en animacion de vertices.");
        std::string value(length,'\0');
        if(length)input.read(&value[0],length);
        if(!input)throw std::runtime_error("Cadena truncada en animacion de vertices.");
        return value;
    }
    void LoadVertexAnimation(const std::string& path)
    {
        std::cout<<"Cargando animacion de vertices "<<path<<"..."<<std::endl;
        const size_t separator=path.find_last_of("/\\");
        directory=separator==std::string::npos?".":path.substr(0,separator);
        std::ifstream input(path,std::ios::binary);
        if(!input)throw std::runtime_error("No se pudo abrir la animacion de vertices: "+path);
        char magic[8]{};input.read(magic,sizeof(magic));
        if(!input||std::memcmp(magic,"XOLXANM1",8)!=0)throw std::runtime_error("Firma invalida en animacion de vertices.");
        const uint32_t version=ReadBinaryValue<uint32_t>(input);
        bakedDuration=ReadBinaryValue<float>(input);
        const uint32_t frameCount=ReadBinaryValue<uint32_t>(input);
        const uint32_t meshCount=ReadBinaryValue<uint32_t>(input);
        if(version!=1||bakedDuration<=0.0f||frameCount<2||frameCount>240||meshCount==0||meshCount>32)
            throw std::runtime_error("Encabezado invalido en animacion de vertices.");
        meshes.reserve(meshCount);bakedMeshes.reserve(meshCount);
        for(uint32_t m=0;m<meshCount;++m)
        {
            const std::string meshName=ReadBinaryString(input);
            const std::string materialName=ReadBinaryString(input);
            const std::string texturePath=ReadBinaryString(input);
            float diffuse[3]{},emissive[3]{},opacity=1.0f;
            input.read(reinterpret_cast<char*>(diffuse),sizeof(diffuse));
            input.read(reinterpret_cast<char*>(emissive),sizeof(emissive));
            input.read(reinterpret_cast<char*>(&opacity),sizeof(opacity));
            if(!input)throw std::runtime_error("Material truncado en animacion de vertices.");
            const uint32_t vertexCount=ReadBinaryValue<uint32_t>(input);
            const uint32_t indexCount=ReadBinaryValue<uint32_t>(input);
            if(vertexCount<3||vertexCount>1000000||indexCount<3||indexCount>3000000||indexCount%3)
                throw std::runtime_error("Topologia invalida en animacion de vertices: "+meshName);
            std::vector<Vertex> base(vertexCount);
            glm::vec3 meshLo((std::numeric_limits<float>::max)()),meshHi((std::numeric_limits<float>::lowest)());
            for(auto& vertex:base)
            {
                float values[8]{};input.read(reinterpret_cast<char*>(values),sizeof(values));
                if(!input)throw std::runtime_error("Vertice base truncado en animacion de vertices.");
                vertex.Position={values[0],values[1],values[2]};
                vertex.Normal={values[3],values[4],values[5]};
                vertex.TexCoords={values[6],values[7]};
                meshLo=glm::min(meshLo,vertex.Position);meshHi=glm::max(meshHi,vertex.Position);
            }
            std::vector<GLuint> indices(indexCount);
            input.read(reinterpret_cast<char*>(indices.data()),static_cast<std::streamsize>(indices.size()*sizeof(GLuint)));
            if(!input)throw std::runtime_error("Indices truncados en animacion de vertices.");
            BakedMeshFrames baked;baked.base=base;baked.frames.resize(frameCount);
            for(uint32_t frame=0;frame<frameCount;++frame)
            {
                auto& values=baked.frames[frame];values.resize(vertexCount);
                for(uint32_t v=0;v<vertexCount;++v)
                {
                    float pose[6]{};input.read(reinterpret_cast<char*>(pose),sizeof(pose));
                    if(!input)throw std::runtime_error("Pose truncada en animacion de vertices.");
                    values[v].position={pose[0],pose[1],pose[2]};values[v].normal={pose[3],pose[4],pose[5]};
                    meshLo=glm::min(meshLo,values[v].position);meshHi=glm::max(meshHi,values[v].position);
                }
            }
            MeshMaterial material;material.diffuseColor={diffuse[0],diffuse[1],diffuse[2]};
            material.emissiveColor={emissive[0],emissive[1],emissive[2]};material.opacity=opacity;
            if(!texturePath.empty())material.diffuseTexture=LoadTexture(texturePath);
            meshes.emplace_back(base,indices,material,meshLo,meshHi,std::vector<SkinVertex>{},glm::mat4(1),false,true);
            minBounds=glm::min(minBounds,meshLo);maxBounds=glm::max(maxBounds,meshHi);
            bakedMeshes.push_back(std::move(baked));
            std::cout<<"  "<<meshName<<" / "<<materialName<<": "<<vertexCount<<" vertices."<<std::endl;
        }
        std::cout<<"Animacion lista: "<<meshCount<<" mallas, "<<frameCount<<" poses, "
            <<bakedDuration<<" s."<<std::endl;
        UpdateBakedAnimation(0.0f);
    }
};
