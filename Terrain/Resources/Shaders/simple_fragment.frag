#version 460

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;

//layout (set = 2, binding = 0) uniform sampler2D Tex;

//layout (set = 2, binding = 1) uniform texture2D Grass;
//layout (set = 2, binding = 2) uniform texture2D Slope;
//layout (set = 2, binding = 3) uniform texture2D Rock;
//layout (set = 2, binding = 4) uniform sampler textureSampler;

void main() 
{
    //vec4 composition = texture(Tex, fragTexCoord);

    vec4 finalColor = vec4(1.0, 0.2, 0.5, 1.0);
    //finalColor += texture(sampler2D(Grass, textureSampler), texCoord) * composition.g;
    //finalColor += texture(sampler2D(Slope, textureSampler), texCoord) * composition.b;
    //finalColor += texture(sampler2D(Rock, textureSampler), texCoord)  * composition.r;

    outColor = finalColor;
    outColor.a = 1.0;
}