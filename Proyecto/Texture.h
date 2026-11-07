#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "stb_image.h"

class TextureLoading
{
    using Faces = std::array<std::vector<unsigned char>, 6>;
    static glm::vec3 Direction(int face, float u, float v)
    {
        switch (face)
        {
        case 0: return { 1, -v, -u };
        case 1: return { -1, -v, u };
        case 2: return { u, 1, v };
        case 3: return { u, -1, -v };
        case 4: return { u, -v, 1 };
        default: return { -u, -v, -1 };
        }
    }
    static glm::vec2 UV(int face, const glm::vec3& d)
    {
        switch (face)
        {
        case 0: return glm::vec2(-d.z, -d.y) / std::abs(d.x);
        case 1: return glm::vec2(d.z, -d.y) / std::abs(d.x);
        case 2: return glm::vec2(d.x, d.z) / std::abs(d.y);
        case 3: return glm::vec2(d.x, -d.z) / std::abs(d.y);
        case 4: return glm::vec2(d.x, -d.y) / std::abs(d.z);
        default: return glm::vec2(-d.x, -d.y) / std::abs(d.z);
        }
    }
    static glm::vec3 SharedEdge(const Faces& faces, int size, const glm::vec3& direction)
    {
        const glm::vec3 a = glm::abs(direction);
        const float major = std::max(a.x, std::max(a.y, a.z));
        glm::vec3 color(0.0f);
        float count = 0.0f;
        for (int face = 0; face < 6; ++face)
        {
            const int axis = face / 2;
            const float signedAxis = direction[axis] * (face % 2 == 0 ? 1.0f : -1.0f);
            if (signedAxis < major - 0.00001f) continue;
            const glm::vec2 uv = UV(face, direction);
            const int x = glm::clamp(static_cast<int>(std::round((uv.x + 1.0f) * 0.5f * (size - 1))), 0, size - 1);
            const int y = glm::clamp(static_cast<int>(std::round((uv.y + 1.0f) * 0.5f * (size - 1))), 0, size - 1);
            const size_t index = (static_cast<size_t>(y) * size + x) * 3;
            color += glm::vec3(faces[face][index], faces[face][index + 1], faces[face][index + 2]);
            count += 1.0f;
        }
        return color / std::max(1.0f, count);
    }
    static void StitchEdges(Faces& faces, int size)
    {
        // CLAMP y SEAMLESS no corrigen diferencias pintadas en las imagenes.
        // Igualar parejas de bordes y esquinas triples sin cambiar los originales.
        const Faces source = faces;
        const int band = std::min(12, size / 4);
        if (band == 0) return;
        for (int face = 0; face < 6; ++face)
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x)
                {
                    const int dx = std::min(x, size - 1 - x), dy = std::min(y, size - 1 - y);
                    const int distance = std::min(dx, dy);
                    if (distance >= band) continue;
                    float u = 2.0f * x / (size - 1) - 1.0f;
                    float v = 2.0f * y / (size - 1) - 1.0f;
                    if (dx <= dy) u = x < size / 2 ? -1.0f : 1.0f;
                    if (dy <= dx) v = y < size / 2 ? -1.0f : 1.0f;
                    const glm::vec3 shared = SharedEdge(source, size, Direction(face, u, v));
                    const float t = static_cast<float>(distance) / band;
                    const float blend = 1.0f - t * t * (3.0f - 2.0f * t);
                    const size_t index = (static_cast<size_t>(y) * size + x) * 3;
                    for (int c = 0; c < 3; ++c)
                        faces[face][index + c] = static_cast<unsigned char>(std::round(
                            glm::mix(static_cast<float>(source[face][index + c]), shared[c], blend)));
                }
    }
public:
    static GLuint LoadCubemap(const std::vector<const GLchar*>& filenames)
    {
        if (filenames.size() != 6) throw std::runtime_error("El skybox requiere seis caras.");
        Faces faces;
        int size = 0;
        for (size_t i = 0; i < 6; ++i)
        {
            int width = 0, height = 0, channels = 0;
            unsigned char* data = stbi_load(filenames[i], &width, &height, &channels, STBI_rgb);
            if (!data || width != height || width < 2 || (size && width != size))
            {
                stbi_image_free(data);
                throw std::runtime_error("Cara del skybox incompatible: " + std::string(filenames[i]));
            }
            size = width;
            faces[i].assign(data, data + static_cast<size_t>(size) * size * 3);
            stbi_image_free(data);
        }
        StitchEdges(faces, size);
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        GLint previousAlignment;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (size_t i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(i), 0,
                GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, faces[i].data());
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
            static_cast<GLint>(std::floor(std::log2(size))));
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return texture;
    }
};
