#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D heightMap;

void main() 
{
    vec3 position = inPosition;
    position.y = texture(heightMap, inTexCoord).r * 10.0f;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0);
}