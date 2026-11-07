#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D texture_diffuse1;
uniform vec3 tint;
void main()
{
    vec4 texColor = texture(texture_diffuse1, TexCoords);
    if (texColor.a < 0.1) discard;
    FragColor = vec4(texColor.rgb * tint, texColor.a);
}
