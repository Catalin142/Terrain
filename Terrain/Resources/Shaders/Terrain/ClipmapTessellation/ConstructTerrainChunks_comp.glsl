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

layout(std430, set = 0, binding = 2) readonly buffer TessellationSettings
{
    int ControlPointSize;
    int ControlPointsPerRow;
};

layout(set = 0, binding = 3) uniform TerrainInfoUniformBuffer
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

struct VkDrawIndirectCommand 
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

layout(std430, set = 2, binding = 0) writeonly buffer _DrawCommand
{
    VkDrawIndirectCommand drawCommand;
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
    drawCommand.vertexCount = ControlPointsPerRow * ControlPointsPerRow * 4;
    drawCommand.instanceCount = 0;
    drawCommand.firstVertex = 0;
    drawCommand.firstInstance = 0;

    barrier();

    uvec2 position = uvec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    uint lod = gl_GlobalInvocationID.z;

    LODMargins lodMargin = Margins.margin[lod];

    int patchSize = ControlPointsPerRow * ControlPointSize;

    vec2 chunkPosition = vec2(lodMargin.xMargins.x + position.x, lodMargin.yMargins.x  + position.y);
    int chunkSize = patchSize << lod;
    
	uint outDirection = 0;
        
    outDirection |= (position.x == 0) ? 1u : 0;  
    outDirection |= (position.x == patchSize - 1) ? 2u : 0; 
    outDirection |= (position.y == 0) ? 4u : 0; 
    outDirection |= (position.y == patchSize - 1) ? 8u : 0;
	
    bool remove = false;

    if (lod != 0)
	{
        LODMargins lodMarginPrev = Margins.margin[lod - 1];
		uint marginminY = (lodMarginPrev.yMargins.x / 2);
		uint marginmaxY = (lodMarginPrev.yMargins.y / 2);
		uint marginminX = (lodMarginPrev.xMargins.x / 2);
		uint marginmaxX = (lodMarginPrev.xMargins.y / 2);
            
        bool inYBounds = chunkPosition.y >= marginminY && chunkPosition.y < marginmaxY;
        bool inXBounds = chunkPosition.x >= marginminX && chunkPosition.x < marginmaxX;

        outDirection |= (chunkPosition.x == marginminX - 1 && inYBounds) ? 2u : 0;  
        outDirection |= (chunkPosition.x == marginmaxX && inYBounds) ? 1u : 0; 
        outDirection |= (chunkPosition.y == marginmaxY && inXBounds) ? 4u : 0; 
        outDirection |= (chunkPosition.y == marginminY - 1 && inXBounds) ? 8u : 0;

        if (chunkPosition.x >= marginminX && chunkPosition.x < marginmaxX && chunkPosition.y >= marginminY && chunkPosition.y < marginmaxY)
			remove = true;
    }

    if (remove)// || !frustumCheck(chunkPosition * chunkSize, -terrainInfo.ElevationRange.x * 2.0, -terrainInfo.ElevationRange.y * 2.0, chunkSize))
        return;
        
    TerrainChunk chunk;
    chunk.Offset = uint((uint(chunkPosition.x) & 0x0000ffffu) | ((uint(chunkPosition.y) & 0x0000ffffu) << 16));
	chunk.Lod = lod | (outDirection << 16);

    uint index = atomicAdd(drawCommand.instanceCount, 1);
    Chunks.chunk[index] = chunk;
}