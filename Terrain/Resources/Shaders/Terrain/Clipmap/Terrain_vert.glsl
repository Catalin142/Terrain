#version 460 core

layout(location = 0) in ivec2 inPosition;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 terrainUV;

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

layout(set = 0, binding = 1) uniform TerrainInfoUniformBuffer
{
    int Size;
    float heightMultiplier;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

struct LODMargins
{
    ivec2 xMargins;
    ivec2 yMargins;
};

layout(set = 0, binding = 2) readonly buffer TerrainChunksMarginsSet
{
    LODMargins margin[];
} Margins;


layout (set = 1, binding = 0, r16f) uniform readonly image2DArray heightMap;

void main() 
{
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];

    int chunkSize = terrainInfo.minimumChunkSize << chunk.Lod;
    int multiplier = 1 << chunk.Lod;

    ivec2 offset;
    offset.x = int(chunk.Offset & 0x0000ffffu);
    offset.y = int((chunk.Offset >> 16) & 0x0000ffffu);
    
    ivec2 wrappedOffset = offset % 8;

    int paddedSize = terrainInfo.minimumChunkSize + 2;
    wrappedOffset *= paddedSize;
    
    ivec2 xMargin = Margins.margin[chunk.Lod].xMargins;
    ivec2 yMargin = Margins.margin[chunk.Lod].yMargins;
    
    ivec2 position = ivec2(inPosition.x, inPosition.y);
    int isOddVertexZ = position.y & 1;
    int isOddVertexX = position.x & 1;

    position.y += isOddVertexZ * int(position.x == 0) * int(offset.x == xMargin.x);
    position.y += isOddVertexZ * int(position.x == terrainInfo.minimumChunkSize) * int(offset.x == xMargin.y - 1);
    position.x += isOddVertexX * int(position.y == 0) * int(offset.y == yMargin.x);
    position.x += isOddVertexX * int(position.y == terrainInfo.minimumChunkSize) * int(offset.y == yMargin.y - 1);
    
    ivec3 terrainLoadLayer = ivec3(wrappedOffset + position, chunk.Lod);

    position *= multiplier;
    offset *= chunkSize;
    position += offset;
    
    float height = (-imageLoad(heightMap, terrainLoadLayer).r) * terrainInfo.heightMultiplier;

    gl_Position = Camera.Projection * Camera.View * vec4(float(position.x), height, float(position.y), 1.0);

    fragPos = vec3(1.0, 1.0, 1.0);
}