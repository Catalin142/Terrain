#version 460

struct TerrainChunk
{
    uint Offset;
    uint Lod;
};

layout(set = 0, binding = 0) writeonly buffer ChunksStorageBuffer
{
    TerrainChunk chunk[];
} Chunks;

struct LODMargins
{
    ivec2 xMargins;
    ivec2 yMargins;
};

layout(set = 0, binding = 1) readonly buffer TerrainChunksMarginsSet
{
    LODMargins margin[];
} Margins;

layout(set = 0, binding = 2) uniform TerrainInfoUniformBuffer
{
    int Size;
    vec2 ElevationRange;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

layout(std430, set = 1, binding = 0) readonly buffer Camera 
{
	vec4 frustumPlanes[6];
};

struct VkDrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

layout(std430, set = 2, binding = 0) writeonly buffer _DrawCommand
{
    VkDrawIndexedIndirectCommand drawCommand;
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

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    drawCommand.indexCount = 64 * 64 * 24;
    drawCommand.firstIndex = 0;
    drawCommand.vertexOffset = 0;
    drawCommand.firstInstance = 0;

    drawCommand.instanceCount = 0;

    barrier();

    uvec2 position = uvec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    uint lod = gl_GlobalInvocationID.z;

    LODMargins lodMargin = Margins.margin[lod];

    vec2 chunkPosition = vec2(lodMargin.xMargins.x + position.x, lodMargin.yMargins.x + position.y);
    int chunkSize = 128 << lod;
    
	if (lod != 0)
	{
        LODMargins lodMargin = Margins.margin[lod - 1];
		uint marginminY = lodMargin.yMargins.x / 2;
		uint marginmaxY = lodMargin.yMargins.y / 2;
		uint marginminX = lodMargin.xMargins.x / 2;
		uint marginmaxX = lodMargin.xMargins.y / 2;

		if (chunkPosition.x >= marginminX && chunkPosition.x < marginmaxX && chunkPosition.y >= marginminY && chunkPosition.y < marginmaxY)
			return;
    }

    if (!frustumCheck(chunkPosition * chunkSize, -terrainInfo.ElevationRange.x * 2.0, -terrainInfo.ElevationRange.y * 2.0, chunkSize))
        return;
        
    TerrainChunk chunk;
    chunk.Offset = uint((uint(chunkPosition.x) & 0x0000ffffu) | ((uint(chunkPosition.y) & 0x0000ffffu) << 16));
    chunk.Lod = lod;

    uint index = atomicAdd(drawCommand.instanceCount, 1);
    Chunks.chunk[index] = chunk;
}