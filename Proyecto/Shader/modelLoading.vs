#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
out vec2 TexCoords;
out vec3 Normal;
out vec3 WorldPosition;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
void main()
{
    TexCoords = aTexCoords;
    Normal = normalMatrix * aNormal;
    vec4 world = model * vec4(aPos, 1.0);
    WorldPosition = world.xyz;
    gl_Position = projection * view * world;
}
