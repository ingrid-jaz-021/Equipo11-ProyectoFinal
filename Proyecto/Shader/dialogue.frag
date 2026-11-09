#version 330 core
in vec2 TexCoords;
uniform sampler2D dialogueTexture;
out vec4 FragColor;
void main(){FragColor=texture(dialogueTexture,TexCoords);}
