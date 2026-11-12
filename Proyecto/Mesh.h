#pragma once
#include <array>
#include <cstddef>
#include <cmath>
#include <map>
#include <limits>
#include <stdexcept>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Skinning.h"
#include "RenderMetrics.h"
#include "Frustum.h"

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct MeshMaterial
{
    GLuint diffuseTexture = 0; // Pertenece al Model; no se duplica entre mallas.
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    glm::vec3 emissiveColor = glm::vec3(0.0f);
    GLfloat opacity = 1.0f;
};

class Mesh
{
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices,
        const MeshMaterial& material, const glm::vec3& minBounds, const glm::vec3& maxBounds,
        const std::vector<SkinVertex>& skin = {},const glm::mat4& localTransform=glm::mat4(1),bool spatial=false,bool dynamic=false)
        : material(material), minBounds(minBounds), maxBounds(maxBounds),
        indexCount(static_cast<GLsizei>(indices.size())),vertexCount(vertices.size()),localTransform(localTransform)
    {
        localNormal=glm::transpose(glm::inverse(glm::mat3(localTransform)));identityTransform=localTransform==glm::mat4(1);
        std::vector<GLuint> reordered;
        if(spatial&&skin.empty()&&indices.size()>12000)BuildChunks(vertices,indices,reordered);
        const auto& gpuIndices=reordered.empty()?indices:reordered;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), dynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, gpuIndices.size() * sizeof(GLuint), gpuIndices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, Normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, TexCoords)));
        if (!skin.empty())
        {
            glGenBuffers(1, &skinVBO);
            glBindBuffer(GL_ARRAY_BUFFER, skinVBO);
            glBufferData(GL_ARRAY_BUFFER, skin.size() * sizeof(SkinVertex), skin.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 4, GL_INT, sizeof(SkinVertex), reinterpret_cast<void*>(offsetof(SkinVertex, joints)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinVertex), reinterpret_cast<void*>(offsetof(SkinVertex, weights)));
        }
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // Liberar los vectores temporales al salir: no duplicar la geometria de la GPU en RAM.
    }

    ~Mesh() { Release(); }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept : material(other.material), minBounds(other.minBounds),
        maxBounds(other.maxBounds), indexCount(other.indexCount),vertexCount(other.vertexCount),
        VAO(other.VAO), VBO(other.VBO), EBO(other.EBO), skinVBO(other.skinVBO),localTransform(other.localTransform),
        localNormal(other.localNormal),identityTransform(other.identityTransform),chunks(std::move(other.chunks))
    {
        configuredInstanceBuffer=other.configuredInstanceBuffer;
        other.VAO = other.VBO = other.EBO = 0;
        other.skinVBO = 0;
    }
    Mesh& operator=(Mesh&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            material = other.material; indexCount = other.indexCount;vertexCount=other.vertexCount;
            minBounds = other.minBounds; maxBounds = other.maxBounds;
            VAO = other.VAO; VBO = other.VBO; EBO = other.EBO;
            skinVBO = other.skinVBO;
            localTransform=other.localTransform;
            localNormal=other.localNormal;identityTransform=other.identityTransform;chunks=std::move(other.chunks);configuredInstanceBuffer=other.configuredInstanceBuffer;
            other.VAO = other.VBO = other.EBO = 0;
            other.skinVBO = 0;
        }
        return *this;
    }

    bool IsVisible(const std::array<glm::vec4, 6>& planes) const
    {
        return BoundsVisible(planes,minBounds,maxBounds);
    }
    const glm::mat4& LocalTransform() const{return localTransform;}
    const glm::mat3& LocalNormal() const{return localNormal;}
    bool HasIdentityTransform() const{return identityTransform;}
    void UpdateVertices(const std::vector<Vertex>& vertices)
    {
        if(vertices.size()!=vertexCount)throw std::runtime_error("La secuencia de malla cambio su topologia.");
        glBindBuffer(GL_ARRAY_BUFFER,VBO);
        // Orphaning evita esperar al frame anterior; mismo VBO y misma geometria.
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),nullptr,GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(Vertex),vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    void Draw(const Shader& shader,const FrustumPlanes* planes=nullptr) const
    {
        counts.clear();offsets.clear();uint64_t visibleIndices=0;
        if(planes&&!chunks.empty())
        {
            for(const auto& chunk:chunks)
                if(BoundsVisible(*planes,chunk.lo,chunk.hi))
                {counts.push_back(chunk.count);offsets.push_back(reinterpret_cast<const void*>(chunk.byteOffset));visibleIndices+=chunk.count;}
                else ++RenderMetrics::culled;
            if(counts.empty())return;
        }
        BindMaterial(shader);RenderState::BindVao(VAO);
        if(counts.empty()){glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,nullptr);RenderMetrics::Draw(indexCount);}
        else
        {
            glMultiDrawElements(GL_TRIANGLES,counts.data(),GL_UNSIGNED_INT,offsets.data(),static_cast<GLsizei>(counts.size()));
            RenderMetrics::Draw(visibleIndices);
        }
    }
    void DrawInstanced(const Shader& shader,GLuint instanceBuffer,GLsizei count) const
    {
        if(!count)return;
        BindMaterial(shader);RenderState::BindVao(VAO);
        if(configuredInstanceBuffer!=instanceBuffer)
        {
            glBindBuffer(GL_ARRAY_BUFFER,instanceBuffer);
            for(GLuint col=0;col<4;++col)
            {
                glEnableVertexAttribArray(3+col);glVertexAttribPointer(3+col,4,GL_FLOAT,GL_FALSE,sizeof(glm::mat4),reinterpret_cast<void*>(sizeof(glm::vec4)*col));
                glVertexAttribDivisor(3+col,1);
            }
            configuredInstanceBuffer=instanceBuffer;
        }
        glDrawElementsInstanced(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,nullptr,count);RenderMetrics::Draw(indexCount,count);
    }

private:
    MeshMaterial material;
    glm::vec3 minBounds, maxBounds;
    GLsizei indexCount = 0;
    size_t vertexCount=0;
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLuint skinVBO = 0;
    mutable GLuint configuredInstanceBuffer=0;
    glm::mat4 localTransform=glm::mat4(1);
    glm::mat3 localNormal=glm::mat3(1);bool identityTransform=true;
    struct Chunk{glm::vec3 lo,hi;GLsizei count;uintptr_t byteOffset;};
    std::vector<Chunk> chunks;
    mutable std::vector<GLsizei> counts;mutable std::vector<const void*> offsets;
    void BindMaterial(const Shader& shader) const
    {
        RenderState::BindTexture2D(material.diffuseTexture);
        if(RenderState::material==&material)return;
        glUniform1i(shader.Uniform("material.hasDiffuseTexture"),material.diffuseTexture!=0);
        glUniform3fv(shader.Uniform("material.diffuseColor"),1,&material.diffuseColor[0]);
        glUniform3fv(shader.Uniform("material.emissiveColor"),1,&material.emissiveColor[0]);
        glUniform1f(shader.Uniform("material.opacity"),material.opacity);RenderState::material=&material;
    }
    void BuildChunks(const std::vector<Vertex>& vertices,const std::vector<GLuint>& indices,std::vector<GLuint>& reordered)
    {
        struct Bin{std::vector<GLuint> indices;glm::vec3 lo=glm::vec3((std::numeric_limits<float>::max)()),hi=glm::vec3((std::numeric_limits<float>::lowest)());};
        std::map<std::pair<int,int>,Bin> bins;
        for(size_t i=0;i+2<indices.size();i+=3)
        {
            const auto p=(vertices[indices[i]].Position+vertices[indices[i+1]].Position+vertices[indices[i+2]].Position)/3.0f;
            auto& bin=bins[{static_cast<int>(std::floor(p.x/16)),static_cast<int>(std::floor(p.z/16))}];
            for(int j=0;j<3;++j){const auto id=indices[i+j];bin.indices.push_back(id);bin.lo=glm::min(bin.lo,vertices[id].Position);bin.hi=glm::max(bin.hi,vertices[id].Position);}
        }
        reordered.reserve(indices.size());chunks.reserve(bins.size());counts.reserve(bins.size());offsets.reserve(bins.size());
        for(const auto& item:bins)
        {
            const auto& bin=item.second;chunks.push_back({bin.lo,bin.hi,static_cast<GLsizei>(bin.indices.size()),reordered.size()*sizeof(GLuint)});
            reordered.insert(reordered.end(),bin.indices.begin(),bin.indices.end());
        }
    }
    void Release()
    {
        if (skinVBO) glDeleteBuffers(1, &skinVBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
    }
};
