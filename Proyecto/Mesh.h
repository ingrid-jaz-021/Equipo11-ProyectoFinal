#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Skinning.h"

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
        const std::vector<SkinVertex>& skin = {},const glm::mat4& localTransform=glm::mat4(1))
        : material(material), minBounds(minBounds), maxBounds(maxBounds),
        indexCount(static_cast<GLsizei>(indices.size())),localTransform(localTransform)
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
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
        maxBounds(other.maxBounds), indexCount(other.indexCount),
        VAO(other.VAO), VBO(other.VBO), EBO(other.EBO), skinVBO(other.skinVBO),localTransform(other.localTransform)
    {
        other.VAO = other.VBO = other.EBO = 0;
        other.skinVBO = 0;
    }
    Mesh& operator=(Mesh&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            material = other.material; indexCount = other.indexCount;
            minBounds = other.minBounds; maxBounds = other.maxBounds;
            VAO = other.VAO; VBO = other.VBO; EBO = other.EBO;
            skinVBO = other.skinVBO;
            localTransform=other.localTransform;
            other.VAO = other.VBO = other.EBO = 0;
            other.skinVBO = 0;
        }
        return *this;
    }

    bool IsVisible(const std::array<glm::vec4, 6>& planes) const
    {
        for (const glm::vec4& plane : planes)
        {
            const glm::vec3 positive(plane.x >= 0.0f ? maxBounds.x : minBounds.x,
                plane.y >= 0.0f ? maxBounds.y : minBounds.y,
                plane.z >= 0.0f ? maxBounds.z : minBounds.z);
            if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f) return false;
        }
        return true;
    }
    const glm::mat4& LocalTransform() const{return localTransform;}

    void Draw(const Shader& shader) const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.diffuseTexture);
        glUniform1i(glGetUniformLocation(shader.Program, "material.hasDiffuseTexture"), material.diffuseTexture != 0);
        glUniform3fv(glGetUniformLocation(shader.Program, "material.diffuseColor"), 1, &material.diffuseColor[0]);
        glUniform3fv(glGetUniformLocation(shader.Program, "material.emissiveColor"), 1, &material.emissiveColor[0]);
        glUniform1f(glGetUniformLocation(shader.Program, "material.opacity"), material.opacity);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }

private:
    MeshMaterial material;
    glm::vec3 minBounds, maxBounds;
    GLsizei indexCount = 0;
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLuint skinVBO = 0;
    glm::mat4 localTransform=glm::mat4(1);
    void Release()
    {
        if (skinVBO) glDeleteBuffers(1, &skinVBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
    }
};
