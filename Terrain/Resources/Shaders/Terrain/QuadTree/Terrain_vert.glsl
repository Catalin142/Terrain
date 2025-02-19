#version 460 core

layout(location = 0) in ivec2 inPosition;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 terrainUV;
layout(location = 2) flat out vec2 cameraPosition;

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

struct TerrainChunk
{
    uint Offset;
    uint Lod;
    uint ChunkPhysicalLocation;
    uint NeighboursLod;
};

layout(std430, set = 0, binding = 0) readonly buffer ChunksStorageBuffer
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

layout (set = 0, binding = 2, r8ui) uniform readonly uimage2D LODMap;

layout (set = 1, binding = 0, r16f) uniform readonly image2D heightMapPhysicalTexture;
layout (set = 1, binding = 1, r32ui) uniform readonly uimage2D indirectionTexture[6];

void main() 
{
    ivec2 position = inPosition;
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];

    ivec2 offset;
    offset.x = int(chunk.Offset & 0x0000ffffu);
    offset.y = int((chunk.Offset >> 16) & 0x0000ffffu);

    int currentLod = int(chunk.Lod);
    int upLod      = int(chunk.NeighboursLod & 0xff000000u) >> 24;
    int downLod    = int(chunk.NeighboursLod & 0x00ff0000u) >> 16;
    int leftLod    = int(chunk.NeighboursLod & 0x0000ff00u) >> 8;
    int rightLod   = int(chunk.NeighboursLod & 0x000000ffu);
    
    int chunkSize = terrainInfo.minimumChunkSize << chunk.Lod;
    int multiplier = chunkSize / terrainInfo.minimumChunkSize;

    position *= multiplier;

    position.x += (upLod    - (position.x % upLod))     * int(position.y == chunkSize) * int((position.x % upLod) != 0);
    position.x += (downLod  - (position.x % downLod))   * int(position.y == 0)         * int((position.x % downLod) != 0);
    position.y += (rightLod - (position.y % rightLod))  * int(position.x == chunkSize) * int((position.y % rightLod) != 0);
    position.y += (leftLod  - (position.y % leftLod))   * int(position.x == 0)         * int((position.y % leftLod) != 0);

    ivec2 terrainPhLoc = position;
        
    offset *= chunkSize;
    position += offset;

    ivec2 chunkPhysicalLocation;
    chunkPhysicalLocation.x = (terrainInfo.minimumChunkSize + 2) * int(chunk.ChunkPhysicalLocation & 0x0000ffffu);
    chunkPhysicalLocation.y = (terrainInfo.minimumChunkSize + 2) * int((chunk.ChunkPhysicalLocation >> 16) & 0x0000ffffu);
    
    terrainPhLoc /= 1 << chunk.Lod;
    terrainPhLoc += chunkPhysicalLocation;
    
    ivec2 incrementPos;
    incrementPos.x = position.x == 0 ? 1 : 0;
    incrementPos.y = position.y == 0 ? 1 : 0;

    terrainPhLoc += incrementPos;
    position += incrementPos;
    
    float height = -imageLoad(heightMapPhysicalTexture, ivec2(terrainPhLoc.x, terrainPhLoc.y)).r;

    gl_Position = Camera.Projection * Camera.View * vec4(float(position.x), height * terrainInfo.heightMultiplier, float(position.y), 1.0);
    
    //fragPos = vec3(1.0);
    //if (height > 0.0)
    //    fragPos = vec3(0.0, 0.0, abs(height));
    //else
    fragPos = vec3(0.0, abs(height), 0.0);

}