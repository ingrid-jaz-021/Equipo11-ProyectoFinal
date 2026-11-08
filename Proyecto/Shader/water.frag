#version 330 core
in vec2 TexCoords;
in vec3 WorldPosition;
out vec4 FragColor;
uniform sampler2D texture_diffuse1;
uniform float time;
uniform vec3 eyePosition;
vec2 patchOffset(vec2 cell)
{
    return fract(sin(vec2(dot(cell, vec2(127.1,311.7)),
                          dot(cell, vec2(269.5,183.3)))) * 43758.5453) * 2.0;
}
vec3 sampleWater(vec2 uv)
{
    // La fotografia original no es tileable. Cuatro parches con offsets
    // estables y bordes compartidos eliminan la cuadricula de repeticiones.
    vec2 cell = floor(uv), blend = smoothstep(vec2(0.0),vec2(1.0),fract(uv));
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec3 a = textureGrad(texture_diffuse1,uv+patchOffset(cell),dx,dy).rgb;
    vec3 b = textureGrad(texture_diffuse1,uv+patchOffset(cell+vec2(1,0)),dx,dy).rgb;
    vec3 c = textureGrad(texture_diffuse1,uv+patchOffset(cell+vec2(0,1)),dx,dy).rgb;
    vec3 d = textureGrad(texture_diffuse1,uv+patchOffset(cell+vec2(1,1)),dx,dy).rgb;
    return mix(mix(a,b,blend.x),mix(c,d,blend.x),blend.y);
}
void main()
{
    vec2 ripple = vec2(sin(TexCoords.y * 6.1 + time * 0.75),
                       cos(TexCoords.x * 5.3 - time * 0.62));
    vec2 uvA = TexCoords + vec2(0.014, 0.006) * time + ripple * 0.012;
    vec2 uvB = TexCoords * 1.071 + vec2(-0.009, 0.011) * time - ripple * 0.008;
    vec3 albedo = mix(sampleWater(uvA),sampleWater(uvB),0.28);
    vec3 normal = normalize(vec3(ripple.x * 0.045, 1.0, ripple.y * 0.045));
    vec3 viewDirection = normalize(eyePosition - WorldPosition);
    float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
    vec3 color = mix(albedo * 0.76, vec3(0.075, 0.13, 0.19), fresnel * 0.34);
    // El agua escribe profundidad; las plataformas y esculturas conservan su
    // oclusion real y el cielo no se transparenta por debajo de la laguna.
    FragColor = vec4(color, 1.0);
}
