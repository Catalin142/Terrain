#version 460

struct GPUQuadTreeRenderChunk
{
    uint Offset;
    uint Lod;
    uint ChunkPhysicalLocation;
    uint NeighboursLod;
};

struct VkDrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct PassMetadata
{
    uint TMPArray1Index;
    uint ResultArrayIndex;
    uint LoadedSize;
};

layout(std430, set = 0, binding = 0) writeonly buffer _DrawCommand
{
    VkDrawIndexedIndirectCommand drawCommand;
};

layout(std430, set = 0, binding = 1) buffer _MetadataSet {
    PassMetadata metadata;
};

layout(set = 0, binding = 2) uniform TerrainInfoUniformBuffer
{
    int Size;
    vec2 ElevationRange;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

layout(std430, set = 1, binding = 0) readonly buffer _InChunks
{
    GPUQuadTreeRenderChunk nodes[];
} InChunks;

layout(std430, set = 1, binding = 1) writeonly buffer _FinalResult
{
    GPUQuadTreeRenderChunk nodes[];
} FinalResult;

layout(std430, set = 2, binding = 0) readonly buffer Camera 
{
	vec4 frustumPlanes[6];
};

bool frustumCheck(vec2 offset, float minHeight, float maxHeight, int size)
{
    vec3 minBound = vec3(offset.x, minHeight, offset.y);
    vec3 maxBound = vec3(offset.x + size, maxHeight, offset.y + size);

    for (int i = 0; i < 6; i++) {
        vec4 g = frustumPlanes[i];
        int outVertices = 0;
        outVertices += int(dot(g, vec4(minBound.x, minBound.y, minBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(maxBound.x, minBound.y, minBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(minBound.x, maxBound.y, minBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(maxBound.x, maxBound.y, minBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(minBound.x, minBound.y, maxBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(maxBound.x, minBound.y, maxBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(minBound.x, maxBound.y, maxBound.z, 1.0)) < 0.0);
        outVertices += int(dot(g, vec4(maxBound.x, maxBound.y, maxBound.z, 1.0)) < 0.0);
        if (outVertices == 8) return false;
    }

    return true;
}

layout(local_size_x = 16, local_size_y = 1) in;
void main()
{
    if (gl_GlobalInvocationID.x == 0)
    {
        drawCommand.indexCount = 64 * 64 * 24;
        drawCommand.firstIndex = 0;
        drawCommand.vertexOffset = 0;
        drawCommand.firstInstance = 0;

        drawCommand.instanceCount = 0;
    }

    barrier();

    if (gl_GlobalInvocationID.x >= metadata.ResultArrayIndex)
        return;

    GPUQuadTreeRenderChunk node = InChunks.nodes[gl_GlobalInvocationID.x];
    
    ivec2 texel;
    texel.x = int(node.Offset & 0x0000ffffu);
    texel.y = int(node.Offset & 0xffff0000u) >> 16;
    
    int chunkSize = 128 << node.Lod;
    vec2 position = texel * chunkSize;

    if (!frustumCheck(position, -terrainInfo.ElevationRange.x * 2.0, -terrainInfo.ElevationRange.y * 2.0, chunkSize))
        return;

    uint index = atomicAdd(drawCommand.instanceCount, 1);
    FinalResult.nodes[index] = node;
}