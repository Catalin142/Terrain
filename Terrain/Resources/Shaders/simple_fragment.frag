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

    vec3 grassColor = texture(Grass, texCoord).rgb * composition.g;
    vec3 slopeColor = texture(Slope, texCoord).rgb * composition.b;
    vec3 rockColor = texture(Rock, texCoord).rgb  * composition.r;

    outColor.rgb = grassColor + slopeColor + rockColor;
    outColor.a = 1.0;
}