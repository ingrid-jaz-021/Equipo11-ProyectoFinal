#version 330 core
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aTexCoords;
layout(location=3) in mat4 instanceModel;
layout(location=7) in vec4 instanceTint;
out vec2 TexCoords;
out vec3 ColorTint;
uniform mat4 viewProjection;
void main()
{
    TexCoords=aTexCoords;
    ColorTint=instanceTint.rgb;
    gl_Position=viewProjection*instanceModel*vec4(aPos,1.0);
}
