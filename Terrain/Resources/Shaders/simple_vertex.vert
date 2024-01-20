#version 450

layout(set = 0, binding = 0) uniform UniformBufferObjectSet
{
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1) uniform OffsetsUniformBuffer
{
    vec2 offset[64];
} offsets;

layout (set = 1, binding = 0) uniform sampler2D heightMap;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 texCoord;

void main() 
{
    vec3 position = vec3(0.0, 0.0, 0.0);
    vec2 offset = offsets.offset[gl_InstanceIndex];

    position.x = floor(gl_VertexIndex / (64.0 + 1));
    position.z = mod(gl_VertexIndex, (64.0 + 1));
    
    texCoord = vec2(position.x / 128.0, position.z / 128.0);

    position.x += offset.x;
    position.z += offset.y;

    vec2 dynamicTexCoord = vec2(position.x / 512.0, position.z / 512.0);

    position.y = (1.0 - texture(heightMap, dynamicTexCoord).r) * 150.0;
    gl_Position = ubo.proj * ubo.view * vec4(position, 1.0);

    fragTexCoord = dynamicTexCoord;
}