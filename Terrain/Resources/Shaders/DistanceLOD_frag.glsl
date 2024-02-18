#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 2) uniform texture2D Composition;
layout (set = 1, binding = 3) uniform texture2D Normals;
layout (set = 2, binding = 0) uniform sampler2DArray texturePack;

vec3 lightDirection = normalize(vec3(10.0, 10.0, 0.0));

void main() 
{
    vec3 composition = texture(sampler2D(Composition, terrainSampler), fragTexCoord).xyz;
    vec3 normal = texture(sampler2D(Normals, terrainSampler), fragTexCoord).xyz;

    float intensity = max(dot(normal, lightDirection), 0.4);

    //vec3 finalColor = vec3(65.0 / 255.0, 152.0 / 255.0, 10.0 / 255.0) * composition.r;
    //finalColor += vec3(71.0 / 255.0, 71.0 / 255.0, 71.0 / 255.0) * composition.g;
    //finalColor += vec3(44.0 / 255.0, 44.0f / 255.0, 44.0 / 255.0) * composition.b;

    vec3 finalColor = texture(texturePack, vec3(texCoord, 0)).rgb;

    outColor = vec4(finalColor * intensity, 1.0);
    outColor.a = 1.0;
}