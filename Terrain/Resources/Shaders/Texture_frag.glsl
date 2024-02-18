#version 460 core

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler texSampler;
layout (set = 0, binding = 1) uniform texture2D geometryPass;

void main() {
    outColor = texture(sampler2D(geometryPass, texSampler), fragTexCoord);
}