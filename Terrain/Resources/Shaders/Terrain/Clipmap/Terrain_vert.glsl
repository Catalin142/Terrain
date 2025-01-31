#version 460 core

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

struct TerrainChunk
{
    uint Offset;
    uint Lod;
};

layout(set = 0, binding = 0) readonly buffer ChunksStorageBufferSet
{
    TerrainChunk chunk[];
} Chunks;

void main() 
{
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];

    vec2 offset;
    offset.x = float(chunk.Offset & 0x0000ffffu);
    offset.y = float((chunk.Offset >> 16) & 0x0000ffffu);

    int chunkSize = 128 << chunk.Lod;
    offset.x *= chunkSize;
    offset.y *= chunkSize;
    
    float multiplier = float(chunkSize) / 128.0;
    int chunkSizePlusOne = 128 + 1;
    position.x = floor(gl_VertexIndex / chunkSizePlusOne) * multiplier;
    position.z = gl_VertexIndex % chunkSizePlusOne * multiplier;

    position.x += offset.x;
    position.z += offset.y;
    
    gl_Position = Camera.Projection * Camera.View * position;
}