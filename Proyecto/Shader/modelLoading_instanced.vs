#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoords;
layout(location=3) in mat4 instanceModel;
out vec2 TexCoords;
out vec3 Normal;
out vec3 WorldPosition;
uniform mat4 viewProjection;
void main()
{
    vec4 world=instanceModel*vec4(aPos,1.0);
    WorldPosition=world.xyz;
    // Cada farola usa la misma escala uniforme: normalizar en el fragmento
    // conserva la normal correcta sin invertir una matriz por vertice.
    Normal=mat3(instanceModel)*aNormal;
    TexCoords=aTexCoords;
    gl_Position=viewProjection*world;
}
