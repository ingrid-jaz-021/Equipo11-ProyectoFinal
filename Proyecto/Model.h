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

struct ModelOptions
{
    GroundSurface* ground = nullptr;
    bool skeletal = false;
    const char* boundaryProfileMaterial = nullptr;
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
    ~Model() { ReleaseTextures(); }
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    void Draw(const Shader& shader, const glm::mat4& clipFromModel, bool cull = true) const
    {
        // Planos del frustum en coordenadas del OBJ: omitir mallas enteras fuera de vista.
        const glm::mat4 rows = glm::transpose(clipFromModel);
        const std::array<glm::vec4, 6> planes = { rows[3] + rows[0], rows[3] - rows[0],
            rows[3] + rows[1], rows[3] - rows[1], rows[3] + rows[2], rows[3] - rows[2] };
        for (const Mesh& mesh : meshes)
            if (!cull || mesh.IsVisible(planes)) mesh.Draw(shader);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    const glm::vec3& GetMinBounds() const { return minBounds; }
    const glm::vec3& GetMaxBounds() const { return maxBounds; }
    SkeletalAnimator* Animator() { return skeleton.get(); }
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
        if(axis<0||axis>2||!options.boundaryProfileMaterial||name!=options.boundaryProfileMaterial||
            !GetMaterialBounds(name,lo,hi)||boundaryProfile.empty()) return false;
        min=glm::vec3((std::numeric_limits<float>::max)());
        max=glm::vec3((std::numeric_limits<float>::lowest)());bool found=false;
        for(const auto& p:boundaryProfile) if(p[axis]>=hi[axis]-band)
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
    std::unordered_map<std::string, GLuint> texturesLoaded;
    std::string directory;
    ModelOptions options;
    std::vector<glm::vec3> boundaryProfile;
    std::unique_ptr<SkeletalAnimator> skeleton;
    struct Bounds { glm::vec3 min, max; };
    std::unordered_map<std::string, Bounds> materialBounds;
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
            minBounds = glm::min(minBounds, boundPosition);
            maxBounds = glm::max(maxBounds, boundPosition);
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
        // Solo se retienen los pocos vertices del tapete solicitado, nunca el
        // escenario masivo. Permite medir su extremo de conexion con la plaza.
        if(options.boundaryProfileMaterial&&materialName==options.boundaryProfileMaterial)
            for(const auto& vertex:vertices) boundaryProfile.push_back(vertex.Position);
        const auto existingBounds = materialBounds.find(materialName);
        if (existingBounds == materialBounds.end()) materialBounds.emplace(materialName, Bounds{ meshMin, meshMax });
        else
        {
            existingBounds->second.min = glm::min(existingBounds->second.min, meshMin);
            existingBounds->second.max = glm::max(existingBounds->second.max, meshMax);
        }
        if (options.ground&&GroundSurface::IsWalkableMaterial(materialName))
        {
            const bool grass=GroundSurface::IsRaisedGrass(materialName);
            for (size_t i = 0; i + 2 < indices.size(); i += 3)
                options.ground->Add(vertices[indices[i]].Position,
                    vertices[indices[i + 1]].Position, vertices[indices[i + 2]].Position,
                    grass);
        }
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
        meshes.emplace_back(vertices, indices, material, meshMin, meshMax, skin);
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
