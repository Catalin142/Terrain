#version 460

struct GPUBruteForceRenderChunk
{
    uint Offset;
    uint Lod;
    uint NeighboursLod;
};

layout(std430, set = 0, binding = 0) readonly buffer ComputePassInfo
{
    vec3 CameraPosition;
    vec4 DistanceThreshold;
};

layout(std430, set = 0, binding = 1) readonly buffer Camera
{
	vec4 frustumPlanes[6];
};

layout(set = 0, binding = 2) uniform TerrainInfoUniformBuffer
{
    int Size;
    vec2 ElevationRange;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

layout(std430, set = 1, binding = 0) writeonly buffer _FinalResult
{
    GPUBruteForceRenderChunk nodes[];
} FinalResult;

layout(std430, set = 1, binding = 1) buffer _ResultCountSet {
    uint ResultNodesCount;
};

struct VkDrawIndirectCommand 
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
} ;

layout(std430, set = 2, binding = 0) writeonly buffer _DrawCommandSet
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

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
    if (gl_GlobalInvocationID.x * gl_GlobalInvocationID.y == 1)
    {
        drawCommand.vertexCount = 4;
        drawCommand.instanceCount = 0;
        drawCommand.firstVertex = 0;
        drawCommand.firstInstance = 0;
    }

    vec2 offset = vec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    uvec2 uoffset = uvec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);

    if (!frustumCheck(offset * terrainInfo.minimumChunkSize, -terrainInfo.ElevationRange.x * 2.0, -terrainInfo.ElevationRange.y * 2.0, terrainInfo.minimumChunkSize))
        return;

    vec2 camPos = CameraPosition.xz;
    
    float dist = distance(offset * terrainInfo.minimumChunkSize, camPos);
    uvec4 thresholds = uvec4(greaterThanEqual(vec4(dist), DistanceThreshold));
    uint lod = thresholds.x + thresholds.y + thresholds.z + thresholds.w;
    
    float upDist = distance((offset + vec2(0.0, 1.0)) * terrainInfo.minimumChunkSize, camPos);
    uvec4 upThresholds = uvec4(greaterThanEqual(vec4(upDist), DistanceThreshold));
    uint upLod = upThresholds.x + upThresholds.y + upThresholds.z + upThresholds.w;

    float downDist = distance((offset + vec2(0.0, -1.0)) * terrainInfo.minimumChunkSize, camPos);
    uvec4 downThresholds = uvec4(greaterThanEqual(vec4(downDist), DistanceThreshold));
    uint downLod = downThresholds.x + downThresholds.y + downThresholds.z + downThresholds.w;
    
    float rightDist = distance((offset + vec2(1.0, 0.0)) * terrainInfo.minimumChunkSize, camPos);
    uvec4 rightThresholds = uvec4(greaterThanEqual(vec4(rightDist), DistanceThreshold));
    uint rightLod  = rightThresholds.x + rightThresholds.y + rightThresholds.z + rightThresholds.w;
    
    float leftDist = distance((offset + vec2(-1.0, 0.0)) * terrainInfo.minimumChunkSize, camPos);
    uvec4 leftThresholds = uvec4(greaterThanEqual(vec4(leftDist), DistanceThreshold));
    uint leftLod = leftThresholds.x + leftThresholds.y + leftThresholds.z + leftThresholds.w;

    uint index = atomicAdd(ResultNodesCount, 1);
    atomicMax(drawCommand.instanceCount, ResultNodesCount);

    GPUBruteForceRenderChunk renderNode;
    renderNode.Offset = (uoffset.x & 0x0000ffffu) | ((uoffset.y & 0x0000ffffu) << 16);
    renderNode.Lod = lod;
    renderNode.NeighboursLod = 
        ((upLod      & 0x0000000fu) << 24) | 
        ((downLod    & 0x0000000fu) << 16) | 
        ((leftLod    & 0x0000000fu) << 8)  | 
        ((rightLod   & 0x0000000fu));

    FinalResult.nodes[index] = renderNode;
}