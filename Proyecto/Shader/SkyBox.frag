#version 330 core
in vec3 TexCoords;
out vec4 color;

uniform samplerCube skybox;

void main()
{
    vec3 direction = normalize(TexCoords);
    // Las caras originales tienen fondos de distinto tono: CLAMP por si solo
    // no puede ocultar esa diferencia. Conservar sus estrellas y reconstruir
    // la nebulosa en coordenadas esfericas, continuas entre todas las caras.
    vec3 detailed = textureLod(skybox, direction, 0.0).rgb;
    vec3 background = textureLod(skybox, direction, 6.0).rgb;
    vec3 stars = max(detailed - background - vec3(0.018), vec3(0.0));
    float cloud = 0.5 + 0.5 * sin(direction.x * 3.0 + direction.y * 2.0
        + 0.8 * sin(direction.z * 4.0));
    float haze = cloud * cloud * (0.65 + 0.35 * cos(direction.z * 3.0 - direction.y));
    vec3 nebula = mix(vec3(0.008, 0.006, 0.032), vec3(0.032, 0.008, 0.045), haze);
    color = vec4(nebula + stars, 1.0);
}
