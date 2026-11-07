#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoords;
layout(location=3) in ivec4 joints;
layout(location=4) in vec4 weights;
out vec2 TexCoords;
out vec3 Normal;
out vec3 WorldPosition;
uniform mat4 bones[48];
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
void main()
{
    mat4 skin = bones[joints.x]*weights.x + bones[joints.y]*weights.y + bones[joints.z]*weights.z + bones[joints.w]*weights.w;
    TexCoords = aTexCoords;
    Normal = normalMatrix * mat3(skin) * aNormal;
    vec4 world = model * skin * vec4(aPos, 1.0);
    WorldPosition = world.xyz;
    gl_Position = projection * view * world;
}
