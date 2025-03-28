#version 450

layout (location = 0) in ivec2 in_Position;

layout(location = 0) out ivec2 out_ControlPointPosition;
layout(location = 1) out uint out_Lod;
layout(location = 2) out ivec2 out_ChunkOffset;
layout(location = 3) out uint out_StitchDirection;

layout(std430, set = 0, binding = 0) readonly buffer TessellationSettings
{
    int ControlPointSize;
    int ControlPointsPerRow;
};

struct TerrainChunk
{
    uint Offset;
    uint Lod;
};

layout(std430, set = 0, binding = 1) readonly buffer ChunksStorageBuffer
{
    TerrainChunk chunk[];
} Chunks;


void main()
{
	TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];

    out_Lod = chunk.Lod & 0x0000FFFFu;
    out_StitchDirection = (chunk.Lod & 0xFFFF0000u) >> 16;

    int quadIndex = gl_VertexIndex / 4;
    int startX = quadIndex % ControlPointsPerRow;
    int startY = quadIndex / ControlPointsPerRow;
    out_ControlPointPosition = ivec2(startX, startY);

    out_ChunkOffset.x = int(chunk.Offset & 0x0000ffffu);
    out_ChunkOffset.y = int((chunk.Offset >> 16) & 0x0000ffffu);

    float cpSize = float(ControlPointSize);
	gl_Position = vec4(in_Position.x * cpSize, 0.0, in_Position.y * cpSize, 1.0);
}