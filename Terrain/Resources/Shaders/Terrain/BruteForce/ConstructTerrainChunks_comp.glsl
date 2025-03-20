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
    float minimumChunkSize;
    vec4 DistanceThreshold;
};

layout(std430, set = 0, binding = 1) readonly buffer Camera
{
	vec4 frustumPlanes[6];
};

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

#define MINMAX_BOUND 400.0

bool frustumCheck(vec2 offset)
{
    vec3 minBound = vec3(offset.x * minimumChunkSize, -MINMAX_BOUND, offset.y * minimumChunkSize);
    vec3 maxBound = vec3(offset.x * minimumChunkSize + minimumChunkSize, MINMAX_BOUND, offset.y * minimumChunkSize + minimumChunkSize);

    vec4 corners[8] = {
        vec4(minBound.x, minBound.y, minBound.z, 1.0),
        vec4(maxBound.x, minBound.y, minBound.z, 1.0),
        vec4(minBound.x, maxBound.y, minBound.z, 1.0),
        vec4(maxBound.x, maxBound.y, minBound.z, 1.0),
        vec4(minBound.x, minBound.y, maxBound.z, 1.0),
        vec4(maxBound.x, minBound.y, maxBound.z, 1.0),
        vec4(minBound.x, maxBound.y, maxBound.z, 1.0),
        vec4(maxBound.x, maxBound.y, maxBound.z, 1.0)
    };

    for (int i = 0; i < 6; ++i) {
        vec4 g = frustumPlanes[i];
        
        bool outside = true;

        for (int cornerIndex = 0; cornerIndex < 8; cornerIndex++)
            if (dot(g, corners[cornerIndex]) >= 0.0)
            {
                outside = false;
                break;
            }
        
        if (outside)
            return false;
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

    if (!frustumCheck(offset))
        return;

    vec2 camPos = CameraPosition.xz;
    
    float dist = distance(offset * minimumChunkSize, camPos);
    uvec4 thresholds = uvec4(greaterThanEqual(vec4(dist), DistanceThreshold));
    uint lod = thresholds.x + thresholds.y + thresholds.z + thresholds.w;
    
    float upDist = distance((offset + vec2(0.0, 1.0)) * minimumChunkSize, camPos);
    uvec4 upThresholds = uvec4(greaterThanEqual(vec4(upDist), DistanceThreshold));
    uint upLod = upThresholds.x + upThresholds.y + upThresholds.z + upThresholds.w;

    float downDist = distance((offset + vec2(0.0, -1.0)) * minimumChunkSize, camPos);
    uvec4 downThresholds = uvec4(greaterThanEqual(vec4(downDist), DistanceThreshold));
    uint downLod = downThresholds.x + downThresholds.y + downThresholds.z + downThresholds.w;
    
    float rightDist = distance((offset + vec2(1.0, 0.0)) * minimumChunkSize, camPos);
    uvec4 rightThresholds = uvec4(greaterThanEqual(vec4(rightDist), DistanceThreshold));
    uint rightLod  = rightThresholds.x + rightThresholds.y + rightThresholds.z + rightThresholds.w;
    
    float leftDist = distance((offset + vec2(-1.0, 0.0)) * minimumChunkSize, camPos);
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