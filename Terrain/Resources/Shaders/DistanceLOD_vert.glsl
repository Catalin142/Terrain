#version 460 core

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 fragmentPosition;

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

struct TerrainChunk
{
    vec2 Offset;
    int Size;
};

layout(set = 0, binding = 0) uniform ChunksUniformBufferSet
{
    TerrainChunk chunk[8 * 8];
} Chunks;

layout(set = 0, binding = 1) uniform LodMapUniformBufferSet
{
    int lod[128 * 128];
} lodMap;

layout(set = 0, binding = 2) uniform TerrainInfoUniformBuffer
{
    vec2 Size;
    float heightMultiplier;
    int minimumChunkSize;
} terrainInfo;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 1) uniform texture2D heightMap;

int getLod(int posX, int posY)
{
    posX = clamp(posX, 0, (int(terrainInfo.Size.x) / terrainInfo.minimumChunkSize) - 1);
    posY = clamp(posY, 0, (int(terrainInfo.Size.y) / terrainInfo.minimumChunkSize) - 1);
    return lodMap.lod[posY * (int(terrainInfo.Size.x) / terrainInfo.minimumChunkSize) + posX];
}

void main() 
{
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];
    vec2 offset = chunk.Offset;
    
    int chunkSizePlusOne = chunk.Size + 1;
    position.x = floor(gl_VertexIndex / chunkSizePlusOne);
    position.z = gl_VertexIndex % chunkSizePlusOne;

    ivec2 chunkPosition = ivec2(int(offset.x) / chunk.Size, int(offset.y) / chunk.Size);
    int currentLod = getLod(chunkPosition.x, chunkPosition.y);
    int upLod      = getLod(chunkPosition.x, chunkPosition.y + 1);
    int downLod    = getLod(chunkPosition.x, chunkPosition.y - 1);
    int rightLod   = getLod(chunkPosition.x + 1, chunkPosition.y);
    int leftLod    = getLod(chunkPosition.x - 1, chunkPosition.y);
    
    float quadsPerChunk = (chunk.Size / currentLod);

    // stiching patches
    position.z += (rightLod - (int(position.z) % rightLod)) * float(position.x == chunk.Size) * float((int(position.z) % rightLod) != 0);
    position.z += (leftLod - (int(position.z) % leftLod)) * float(position.x == 0.0) * float((int(position.z) % leftLod) != 0);
    position.x += (upLod - (int(position.x) % upLod)) * float(position.z == chunk.Size) * float((int(position.x) % upLod) != 0);
    position.x += (downLod - (int(position.x) % downLod)) * float(position.z == 0.0) * float((int(position.x) % downLod) != 0);
        
    texCoord = vec2(position.x / (chunk.Size), position.z / (chunk.Size));
    
    position.x += offset.x;
    position.z += offset.y;
    
    vec2 dynamicTexCoord = vec2(position.x / terrainInfo.Size.x, position.z / terrainInfo.Size.y);
    
    position.y = (-texture(sampler2D(heightMap, terrainSampler), dynamicTexCoord).r) * terrainInfo.heightMultiplier;
    
    gl_Position = Camera.Projection * Camera.View * position;

    fragmentPosition = position.xyz;

    fragTexCoord = dynamicTexCoord;
}