#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 bwColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    outColor =  texture(texSampler, fragTexCoord);
    bwColor.r = (outColor.r + outColor.g + outColor.b) / 6;
    bwColor.g = (outColor.r + outColor.g + outColor.b) / 6;
    bwColor.b = (outColor.r + outColor.g + outColor.b) / 6;
    bwColor.a = 1.0f;
    
    //outColor.rgb = pow(outColor.rgb, vec3(1.0 / 2.2));
}