#version 450

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 1) uniform sampler2D Tex;

layout (set = 2, binding = 0) uniform sampler2D Grass;
layout (set = 2, binding = 1) uniform sampler2D Slope;
layout (set = 2, binding = 2) uniform sampler2D Rock;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;

void main() 
{
    vec4 composition = texture(Tex, fragTexCoord);

    vec3 finalColor = vec3(0.0, 0.0, 0.0);

    finalColor += texture(Grass, texCoord).rgb * composition.g;
    finalColor += texture(Slope, texCoord).rgb * composition.b;
    finalColor += texture(Rock, texCoord).rgb  * composition.r;

    outColor.rgb = finalColor;
    outColor.a = 1.0;
}