#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
in vec3 Normal;
in vec3 WorldPosition;
struct Material
{
    bool hasDiffuseTexture;
    vec3 diffuseColor;
    vec3 emissiveColor;
    float opacity;
};
struct PointLight
{
    vec3 position;
    vec3 color;
    float range;
};
struct SpotLight
{
    vec3 position;
    vec3 direction;
    vec3 color;
    float range;
    float innerCutoff;
    float outerCutoff;
};
uniform Material material;
uniform sampler2D texture_diffuse1;
uniform vec3 lightDirection;
uniform vec3 ambientLight;
uniform vec3 diffuseLight;
uniform PointLight pointLights[12];
uniform int pointLightCount;
uniform vec4 pointLightRegion;
uniform SpotLight spotLights[32];
uniform int spotLightCount;
void main()
{
    vec4 albedo = vec4(material.diffuseColor, material.opacity);
    vec4 texColor = material.hasDiffuseTexture ? texture(texture_diffuse1, TexCoords) : vec4(1.0);
    if(texColor.a < 0.1) discard;
    albedo *= texColor;
    if (albedo.a < 0.1) discard;
    vec3 normal = normalize(Normal);
    if (!gl_FrontFacing) normal = -normal;
    float lambert = max(dot(normal, normalize(-lightDirection)), 0.0);
    vec3 illumination = ambientLight + diffuseLight * lambert;
    // Las farolas se limitan a la plaza. Su radio tiene caida suave y finita:
    // ninguna luz del Zocalo ilumina el Pasillo o la Laguna.
    if (WorldPosition.x >= pointLightRegion.x && WorldPosition.z >= pointLightRegion.y &&
        WorldPosition.x <= pointLightRegion.z && WorldPosition.z <= pointLightRegion.w)
    {
        for (int i = 0; i < min(pointLightCount,12); ++i)
        {
            vec3 delta = pointLights[i].position - WorldPosition;
            float distanceSquared = dot(delta, delta);
            float radius = pointLights[i].range;
            if (distanceSquared >= radius * radius) continue;
            float distanceToLight = sqrt(max(distanceSquared, 0.0001));
            vec3 direction = delta / distanceToLight;
            float normalizedDistance = distanceToLight / radius;
            float cutoff = 1.0 - smoothstep(0.65, 1.0, normalizedDistance);
            float attenuation = cutoff / (1.0 + 2.0 * normalizedDistance + 8.0 * normalizedDistance * normalizedDistance);
            illumination += pointLights[i].color * max(dot(normal, direction), 0.0) * attenuation;
        }
    }
    // Reflectores azules de LuzEsc orientados hacia la piramide de la laguna.
    for (int i = 0; i < min(spotLightCount, 32); ++i)
    {
        vec3 toLight = spotLights[i].position - WorldPosition;
        float distanceToLight = length(toLight);
        float radius = spotLights[i].range;
        if (distanceToLight <= 0.0001 || distanceToLight >= radius) continue;
        vec3 lightDirectionToSurface = -toLight / distanceToLight;
        float coneCos = dot(lightDirectionToSurface, normalize(spotLights[i].direction));
        float cone = smoothstep(spotLights[i].outerCutoff, spotLights[i].innerCutoff, coneCos);
        float surface = max(dot(normal, toLight / distanceToLight), 0.0);
        float normalizedDistance = distanceToLight / radius;
        float attenuation = (1.0 - smoothstep(0.72, 1.0, normalizedDistance)) /
            (1.0 + 1.5 * normalizedDistance + 4.0 * normalizedDistance * normalizedDistance);
        illumination += spotLights[i].color * surface * cone * attenuation;
    }
    FragColor = vec4(albedo.rgb * illumination + material.emissiveColor, 1.0);
}
