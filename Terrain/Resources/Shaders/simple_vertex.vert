#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec2 inCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D heightMap;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 texCoord;

void main() 
{
    vec3 position = inPosition;
    position.y = (1.0 - texture(heightMap, inTexCoord).r) * 150.0;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0);

    fragTexCoord = inTexCoord;
    texCoord = inCoord;
}