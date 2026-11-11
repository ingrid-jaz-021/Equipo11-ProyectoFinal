#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
out vec2 TexCoords;
out vec3 Normal;
out vec3 WorldPosition;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform int guideWalkEnabled;
uniform vec3 guideBoundsMin;
uniform vec3 guideBoundsMax;
uniform float guideWalkPhase;
uniform float guideTravelDirection;

vec3 rotateGuideX(vec3 value, float angle)
{
    float c = cos(angle), s = sin(angle);
    return vec3(value.x, c * value.y - s * value.z, s * value.y + c * value.z);
}

void main()
{
    TexCoords = aTexCoords;
    vec3 localPosition = aPos;
    vec3 localNormal = aNormal;
    if (guideWalkEnabled != 0)
    {
        vec3 extent = max(guideBoundsMax - guideBoundsMin, vec3(0.001));
        vec3 normalizedPosition = (aPos - guideBoundsMin) / extent;
        float legRegion = 1.0 - smoothstep(0.36, 0.55, normalizedPosition.y);
        float leftLeg = (1.0 - smoothstep(0.045, 0.17, abs(normalizedPosition.x - 0.43))) * legRegion;
        float rightLeg = (1.0 - smoothstep(0.045, 0.17, abs(normalizedPosition.x - 0.62))) * legRegion;
        vec3 hipLeft = vec3(guideBoundsMin.x + extent.x * 0.43,
            guideBoundsMin.y + extent.y * 0.53, (guideBoundsMin.z + guideBoundsMax.z) * 0.5);
        vec3 hipRight = vec3(guideBoundsMin.x + extent.x * 0.62,
            guideBoundsMin.y + extent.y * 0.53, (guideBoundsMin.z + guideBoundsMax.z) * 0.5);
        float swing = sin(guideWalkPhase) * 0.48;
        float leftAngle = -swing * guideTravelDirection;
        float rightAngle = swing * guideTravelDirection;
        vec3 rotatedLeft = hipLeft + rotateGuideX(aPos - hipLeft, leftAngle);
        vec3 rotatedRight = hipRight + rotateGuideX(aPos - hipRight, rightAngle);
        localPosition = mix(localPosition, rotatedLeft, leftLeg);
        localPosition = mix(localPosition, rotatedRight, rightLeg);
        localNormal = mix(localNormal, rotateGuideX(localNormal, leftAngle), leftLeg);
        localNormal = mix(localNormal, rotateGuideX(localNormal, rightAngle), rightLeg);
        localPosition.y += abs(sin(guideWalkPhase * 2.0)) * extent.y * 0.018;
    }
    Normal = normalize(normalMatrix * localNormal);
    vec4 world = model * vec4(localPosition, 1.0);
    WorldPosition = world.xyz;
    gl_Position = projection * view * world;
}
