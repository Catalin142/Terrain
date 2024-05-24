#version 460 core

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
    vec2 Offset;
    int Size;
    int Lod;
};

layout(set = 0, binding = 0) uniform ChunksUniformBufferSet
{
    TerrainChunk chunk[8 * 8];
} Chunks;

layout(set = 0, binding = 1) uniform TerrainInfoUniformBuffer
{
    vec2 Size;
    float heightMultiplier;
    int minimumChunkSize;
} terrainInfo;

layout(set = 0, binding = 2) uniform CameraPositionBuffer
{
    vec2 Position;
} cameraInfo;

layout (set = 0, binding = 3, r8ui) uniform readonly uimage2D LODMap;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 1) uniform texture2D heightMap;

int getLod(int posX, int posY)
{
    posX = clamp(posX, 0, (int(terrainInfo.Size.x) / terrainInfo.minimumChunkSize) - 1);
    posY = clamp(posY, 0, (int(terrainInfo.Size.y) / terrainInfo.minimumChunkSize) - 1);
    return int(imageLoad(LODMap, ivec2(posX, posY)).r);
}

void main() 
{
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];
    vec2 offset = chunk.Offset;
    
    float multiplier = float(chunk.Size) / terrainInfo.minimumChunkSize;

    int chunkSizePlusOne = terrainInfo.minimumChunkSize + 1;
    position.x = floor(gl_VertexIndex / chunkSizePlusOne) * multiplier;
    position.z = gl_VertexIndex % chunkSizePlusOne * multiplier;

    ivec2 chunkPosition = ivec2((offset.x) / terrainInfo.minimumChunkSize, (offset.y) / terrainInfo.minimumChunkSize);

    int chunkFarOffset = chunk.Size / terrainInfo.minimumChunkSize;
    int currentLod = chunk.Lod;
    int upLod      = getLod(chunkPosition.x, chunkPosition.y + chunkFarOffset);
    int downLod    = getLod(chunkPosition.x, chunkPosition.y - 1);
    int rightLod   = getLod(chunkPosition.x + chunkFarOffset, chunkPosition.y);
    int leftLod    = getLod(chunkPosition.x - 1, chunkPosition.y);
    
    // stiching patches
    position.z += (rightLod - float(int(position.z) % rightLod)) * float(position.x == chunk.Size) * float((int(position.z) % rightLod) != 0);
    position.z += (leftLod - float(int(position.z) % leftLod)) * float(position.x == 0.0) * float((int(position.z) % leftLod) != 0);
    position.x += (upLod - float(int(position.x) % upLod)) * float(position.z == chunk.Size) * float((int(position.x) % upLod) != 0);
    position.x += (downLod - float(int(position.x) % downLod)) * float(position.z == 0.0) * float((int(position.x) % downLod) != 0);
        
    position.x += offset.x;
    position.z += offset.y;
    
    terrainUV = vec2(position.x / terrainInfo.Size.x, position.z / terrainInfo.Size.y);
    
    position.y = (-texture(sampler2D(heightMap, terrainSampler), terrainUV).r) * terrainInfo.heightMultiplier;

    gl_Position = Camera.Projection * Camera.View * position;

    fragPos = position.xyz;
    cameraPosition = cameraInfo.Position;
}