#version 450

layout (set = 0, binding = 0, r8ui) uniform readonly uimage2D LODMap;

struct GPUQuadTreeRenderChunk
{
    uint Offset;
    uint Lod;
    uint ChunkPhysicalLocation;
    uint NeighboursLod;
};

layout(std430, set = 1, binding = 0) buffer _FinalResult
{
    GPUQuadTreeRenderChunk nodes[];
} FinalResult;

layout(set = 1, binding = 1) uniform TerrainInfoUniformBuffer
{
    int Size;
    float heightMultiplier;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

struct PassMetadata
{
    uint TMPArray1Index;
    uint ResultArrayIndex;
    uint LoadedSize;
};

layout(std430, set = 1, binding = 2) buffer _MetadataSet {
    PassMetadata metadata;
};

layout(local_size_x = 256, local_size_y = 1) in;
void main()
{ 
    if (gl_GlobalInvocationID.x >= metadata.ResultArrayIndex)
        return;
    
    GPUQuadTreeRenderChunk chunk = FinalResult.nodes[gl_GlobalInvocationID.x];

    int chunkFarOffset = 1 << int(chunk.Lod);

    ivec2 offset;
    offset.x = int(chunk.Offset & 0x0000ffffu);
    offset.y = int((chunk.Offset >> 16) & 0x0000ffffu);

    offset *= chunkFarOffset;

    int upRight = terrainInfo.Size / terrainInfo.minimumChunkSize - 1;
    ivec2 marginBottomDown = ivec2(0, 0);
    ivec2 marginUpRight = ivec2(upRight, upRight);

    uint upLod      = imageLoad(LODMap, clamp(ivec2(offset.x, offset.y + chunkFarOffset), marginBottomDown, marginUpRight)).r;
    uint downLod    = imageLoad(LODMap, clamp(ivec2(offset.x, offset.y - 1), marginBottomDown, marginUpRight)).r;
    uint leftLod    = imageLoad(LODMap, clamp(ivec2(offset.x - 1, offset.y), marginBottomDown, marginUpRight)).r;
    uint rightLod   = imageLoad(LODMap, clamp(ivec2(offset.x + chunkFarOffset, offset.y), marginBottomDown, marginUpRight)).r;

    FinalResult.nodes[gl_GlobalInvocationID.x].NeighboursLod = 
        ((upLod      & 0x0000000fu) << 24) | 
        ((downLod    & 0x0000000fu) << 16) | 
        ((leftLod    & 0x0000000fu) << 8)  | 
        ((rightLod   & 0x0000000fu));

}