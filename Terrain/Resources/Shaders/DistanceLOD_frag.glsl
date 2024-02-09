#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout (set = 2, binding = 0) uniform sampler2D Tex;

layout (set = 2, binding = 1) uniform sampler2DArray texturePack;

void main() 
{
    vec4 composition = texture(Tex, fragTexCoord);

    vec4 finalColor = vec4(0.0, 0.0, 0.0, 1.0);
    finalColor += texture(texturePack, vec3(texCoord, 0)) * composition.r;
    finalColor += texture(texturePack, vec3(texCoord, 1)) * composition.g;
    finalColor += texture(texturePack, vec3(texCoord, 2)) * composition.b;

    outColor = finalColor;
    outColor.a = 1.0;
}