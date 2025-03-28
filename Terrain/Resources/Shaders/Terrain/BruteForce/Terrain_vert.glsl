#version 450

layout (location = 0) in ivec2 in_Position;

layout(location = 0) out ivec2 out_ChunkOffset;
layout(location = 1) out uint out_Lod;
layout(location = 2) out uint out_Neightbours;

struct GPUBruteForceRenderChunk
{
    uint Offset;
    uint Lod;
    uint NeighboursLod;
};

layout(std430, set = 0, binding = 0) readonly buffer ChunksStorageBuffer
{
    GPUBruteForceRenderChunk chunk[];
} Chunks;

layout(set = 1, binding = 1) uniform TerrainInfoUniformBuffer
{
    int Size;
    vec2 ElevationRange;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

void main()
{
	GPUBruteForceRenderChunk chunk = Chunks.chunk[gl_InstanceIndex];

    out_Lod = chunk.Lod;
    out_Neightbours = chunk.NeighboursLod;

    out_ChunkOffset.x = int(chunk.Offset & 0x0000ffffu);
    out_ChunkOffset.y = int((chunk.Offset >> 16) & 0x0000ffffu);

    float fChunkSize = float(terrainInfo.minimumChunkSize);
	gl_Position = vec4(in_Position.x * fChunkSize, 0.0, in_Position.y * fChunkSize, 1.0);
}