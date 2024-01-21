#version 460

#define GRID_SIZE 64

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObjectSet
{
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1) uniform OffsetsUniformBufferSet
{
    vec2 offset[256];
} offsets;

layout (set = 1, binding = 0) uniform sampler2D heightMap;

void main() 
{
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    vec2 offset = offsets.offset[gl_InstanceIndex];

    position.x = floor(gl_VertexIndex / (GRID_SIZE + 1));
    position.z = gl_VertexIndex % (GRID_SIZE + 1);
    
    texCoord = vec2(position.x / 128.0, position.z / 128.0);

    position.x += offset.x;
    position.z += offset.y;

    vec2 dynamicTexCoord = vec2(position.x / 1024.0, position.z / 1024.0);

    position.y = (1.0 - texture(heightMap, dynamicTexCoord).r) * 250.0;
    gl_Position = ubo.proj * ubo.view * position;

    fragTexCoord = dynamicTexCoord;
}