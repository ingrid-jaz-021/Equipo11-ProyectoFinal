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
struct TorchPointLight
{
    vec3 position;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float range;
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
uniform TorchPointLight torchLights[2];
uniform int torchLightCount;
uniform vec3 viewPosition;
uniform PointLight ofrendaLights[6];
uniform int ofrendaLightCount;
uniform vec4 ofrendaCandleRegion;
uniform vec3 ofrendaCandleColor;
uniform PointLight fuegoCaminoLights[24];
uniform int fuegoCaminoLightCount;
uniform vec4 fuegoCaminoRegion;
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
        float distanceSquared = dot(toLight,toLight);
        float radius = spotLights[i].range;
        if (distanceSquared <= 0.00000001 || distanceSquared >= radius*radius) continue;
        float distanceToLight = sqrt(distanceSquared);
        vec3 lightDirectionToSurface = -toLight / distanceToLight;
        float coneCos = dot(lightDirectionToSurface, normalize(spotLights[i].direction));
        if(coneCos<=spotLights[i].outerCutoff)continue; // contribucion exactamente cero
        float cone = smoothstep(spotLights[i].outerCutoff, spotLights[i].innerCutoff, coneCos);
        float surface = max(dot(normal, toLight / distanceToLight), 0.0);
        if(surface==0.0)continue;
        float normalizedDistance = distanceToLight / radius;
        float attenuation = (1.0 - smoothstep(0.72, 1.0, normalizedDistance)) /
            (1.0 + 1.5 * normalizedDistance + 4.0 * normalizedDistance * normalizedDistance);
        illumination += spotLights[i].color * surface * cone * attenuation;
    }
    // Torches follow the hand/flame in world space. Unlike plaza lights, they
    // have no fixed region: their finite sphere of influence moves with the NPC.
    vec3 torchSpecular = vec3(0.0);
    vec3 toViewer = viewPosition - WorldPosition;
    vec3 viewDirection = toViewer * inversesqrt(max(dot(toViewer,toViewer),0.00000001));
    for (int i = 0; i < min(torchLightCount, 2); ++i)
    {
        vec3 delta = torchLights[i].position - WorldPosition;
        float distanceSquared = dot(delta, delta);
        float radius = torchLights[i].range;
        if (distanceSquared >= radius * radius) continue;
        float distanceToLight = sqrt(max(distanceSquared, 0.0001));
        vec3 direction = delta / distanceToLight;
        float surface = max(dot(normal, direction),0.0);
        if (surface <= 0.0) continue;
        float cutoff = 1.0 - smoothstep(0.70,1.0,distanceToLight/radius);
        float attenuation = cutoff / max(torchLights[i].constant +
            torchLights[i].linear * distanceToLight +
            torchLights[i].quadratic * distanceSquared,0.0001);
        illumination += torchLights[i].diffuse * surface * attenuation;
        vec3 halfway = direction + viewDirection;
        halfway *= inversesqrt(max(dot(halfway,halfway),0.00000001));
        float highlight = pow(max(dot(normal,halfway),0.0),32.0);
        torchSpecular += torchLights[i].specular * highlight * attenuation;
    }
    // Luces puntuales de la ofrenda (4 lamparas de cubo naranja + veladoras centrales)
    for (int i = 0; i < min(ofrendaLightCount, 6); ++i)
    {
        vec3 delta = ofrendaLights[i].position - WorldPosition;
        float distanceSquared = dot(delta, delta);
        float radius = ofrendaLights[i].range;
        if (distanceSquared >= radius * radius) continue;
        float distanceToLight = sqrt(max(distanceSquared, 0.0001));
        vec3 direction = delta / distanceToLight;
        float normalizedDistance = distanceToLight / radius;
        float cutoff = 1.0 - smoothstep(0.60, 1.0, normalizedDistance);
        float attenuation = cutoff / (1.0 + 1.4 * normalizedDistance + 3.5 * normalizedDistance * normalizedDistance);
        illumination += ofrendaLights[i].color * max(dot(normal, direction), 0.0) * attenuation;
    }
    // Iluminacion tenue ("muy bajito") de las veladoras en la ofrenda
    if (WorldPosition.x >= ofrendaCandleRegion.x && WorldPosition.z >= ofrendaCandleRegion.y &&
        WorldPosition.x <= ofrendaCandleRegion.z && WorldPosition.z <= ofrendaCandleRegion.w &&
        WorldPosition.y >= -15.0 && WorldPosition.y <= 110.0)
    {
        float edgeDistX = min(WorldPosition.x - ofrendaCandleRegion.x, ofrendaCandleRegion.z - WorldPosition.x);
        float edgeDistZ = min(WorldPosition.z - ofrendaCandleRegion.y, ofrendaCandleRegion.w - WorldPosition.z);
        float edgeDist = min(edgeDistX, edgeDistZ);
        float edgeFade = smoothstep(0.0, 35.0, edgeDist);
        illumination += ofrendaCandleColor * edgeFade;
    }
    // Luces puntuales de las canastas de fuego del camino (fuego calido y parpadeo organico a lo largo del pasillo)
    if (fuegoCaminoLightCount > 0 &&
        WorldPosition.x >= fuegoCaminoRegion.x && WorldPosition.z >= fuegoCaminoRegion.y &&
        WorldPosition.x <= fuegoCaminoRegion.z && WorldPosition.z <= fuegoCaminoRegion.w)
    {
        for (int i = 0; i < min(fuegoCaminoLightCount, 24); ++i)
        {
            vec3 delta = fuegoCaminoLights[i].position - WorldPosition;
            float distanceSquared = dot(delta, delta);
            float radius = fuegoCaminoLights[i].range;
            if (distanceSquared >= radius * radius) continue;
            float distanceToLight = sqrt(max(distanceSquared, 0.0001));
            vec3 direction = delta / distanceToLight;
            float normalizedDistance = distanceToLight / radius;
            float cutoff = 1.0 - smoothstep(0.60, 1.0, normalizedDistance);
            float attenuation = cutoff / (1.0 + 1.2 * normalizedDistance + 3.0 * normalizedDistance * normalizedDistance);
            illumination += fuegoCaminoLights[i].color * max(dot(normal, direction), 0.0) * attenuation;
        }
    }
    FragColor = vec4(albedo.rgb * illumination + torchSpecular + material.emissiveColor, 1.0);
}
