#version 450

layout (location = 0) in ivec2 in_Position;

layout(location = 0) out ivec2 out_ControlPointPosition;
layout(location = 2) out uint out_Lod;
layout(location = 3) out ivec2 out_ChunkOffset;

struct TerrainChunk
{
    uint Offset;
    uint Lod;
};

layout(std430, set = 0, binding = 0) readonly buffer ChunksStorageBufferSet
{
    TerrainChunk chunk[];
} Chunks;

void main()
{
	TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex / 64];

    out_Lod = chunk.Lod;
    out_ControlPointPosition = ivec2((gl_InstanceIndex % 64 ) / 8, gl_InstanceIndex % 8);

    out_ChunkOffset.x = int(chunk.Offset & 0x0000ffffu);
    out_ChunkOffset.y = int((chunk.Offset >> 16) & 0x0000ffffu);

	gl_Position = vec4(in_Position.x, 0.0, in_Position.y, 1.0);
}