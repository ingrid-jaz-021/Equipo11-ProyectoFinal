#version 330 core
layout(location=0) in vec2 aPos;
layout(location=2) in vec2 aTexCoords;
uniform vec2 viewportSize;
uniform vec4 screenRect;
out vec2 TexCoords;
void main()
{
    vec2 pixel=screenRect.xy+aPos*screenRect.zw;
    gl_Position=vec4(pixel/viewportSize*2.0-1.0,0.0,1.0);
    TexCoords=aTexCoords;
}
