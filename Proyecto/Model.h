#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
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
#include "stb_image.h"
#include "UnicodeIO.h"
#include "GroundSurface.h"
#include "SkeletalAnimator.h"
#include "Texture2D.h"
#include "FloorUV.h"
#include "MovementSettings.h"

struct ModelOptions
{
    GroundSurface* ground = nullptr;
    bool skeletal = false;
    std::vector<std::string> boundaryProfileMaterials;
};

class Model
{
public:
    explicit Model(const char* path, const std::function<bool()>& keepLoading = {},
        ModelOptions options = {}) : options(options)
    {
        try { LoadModel(path, keepLoading); }
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
        glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer);
        glBufferData(GL_ARRAY_BUFFER,visibleInstances.size()*sizeof(glm::mat4),visibleInstances.data(),GL_STREAM_DRAW);
        for(const auto& mesh:meshes)
        {
            if(!mesh.HasIdentityTransform())throw std::runtime_error("El lote de farolas requiere mallas en el espacio comun del modelo.");
            mesh.DrawInstanced(shader,instanceBuffer,static_cast<GLsizei>(visibleInstances.size()));
        }
    }
    const glm::vec3& GetMinBounds() const { return minBounds; }
    const glm::vec3& GetMaxBounds() const { return maxBounds; }
    SkeletalAnimator* Animator() { return skeleton.get(); }
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
    }
    bool GetMaterialBounds(const std::string& name, glm::vec3& min, glm::vec3& max) const
    {
        const auto found = materialBounds.find(name);
        if (found == materialBounds.end()) return false;
        min = found->second.min; max = found->second.max;
        return true;
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
    mutable std::vector<glm::mat4> visibleInstances;
    std::unordered_map<std::string, GLuint> texturesLoaded;
    std::string directory;
    ModelOptions options;
    std::unordered_map<std::string,std::vector<glm::vec3>> materialProfiles;
    std::unique_ptr<SkeletalAnimator> skeleton;
    struct Bounds { glm::vec3 min, max; };
    std::unordered_map<std::string, Bounds> materialBounds;
    glm::mat4 kioskTransform=glm::mat4(1);
    glm::mat4 fountainTransform=glm::mat4(1);
    float floorLevel=2.005836f,kioskRawHeight=1,fountainRawWidth=1,benchMaxHeight=0;
    size_t benchCount=0;
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
            skeleton = std::make_unique<SkeletalAnimator>(scene);
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
            if (imported->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                aiString texturePath;
                if (imported->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
                {
                    material.diffuseTexture = LoadTexture(texturePath.C_Str());
                    // Blender omite Kd cuando el color procede de una imagen.
                    material.diffuseColor = glm::vec3(1.0f);
                }
            }
        }
        const glm::vec2 tile = options.ground ? ZocaloTileSize(materialName) : glm::vec2(0);
        if (tile.x > 0.0f)
            for (auto& vertex : vertices) vertex.TexCoords = ProjectFloorUV(vertex.Position, vertex.Normal, tile);
        if(options.ground&&materialName=="LoD_0SG")AddBenches(vertices,indices,material,materialName);
        else
        {
            const bool kiosk=options.ground&&materialName=="Material.014";
            const bool fountain=options.ground&&(materialName=="Material.004"||materialName=="Material.005"||materialName=="Material.007");
            AddPart(vertices,indices,skin,material,materialName,kiosk?kioskTransform:fountain?fountainTransform:glm::mat4(1));
        }
    }
    GLuint LoadTexture(std::string relativePath)
    {
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
        const bool absolute = (!relativePath.empty() && relativePath[0] == '/') ||
            (relativePath.size() > 1 && relativePath[1] == ':');
        const std::string filename = absolute ? relativePath : directory + '/' + relativePath;
        const auto existing = texturesLoaded.find(filename);
        if (existing != texturesLoaded.end()) return existing->second;
        const GLuint texture = LoadRgbaTexture(filename);
        texturesLoaded.emplace(filename, texture);
        return texture;
    }
};
