#version 460 core

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 fragmentPosition;
layout(location = 3) flat out int go;
layout(location = 4) flat out ivec2 quadPosition;
layout(location = 5) out vec3 normal;

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
    int go;
} terrainInfo;

layout (set = 0, binding = 2, r8ui) uniform readonly uimage2D LODMap;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 1) uniform texture2D heightMap;

layout (set = 1, binding = 3) uniform texture2D Normals;

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
        
    texCoord = vec2(position.x, position.z);
    
    position.x += offset.x;
    position.z += offset.y;
    
    vec2 dynamicTexCoord = vec2(position.x / terrainInfo.Size.x, position.z / terrainInfo.Size.y);
    
    position.y = (-texture(sampler2D(heightMap, terrainSampler), dynamicTexCoord).r) * terrainInfo.heightMultiplier;
    go = terrainInfo.go;

    gl_Position = Camera.Projection * Camera.View * position;

    fragmentPosition = position.xyz;
    quadPosition = ivec2(fragmentPosition.xz);

    fragTexCoord = dynamicTexCoord;
    
    normal = texture(sampler2D(Normals, terrainSampler), dynamicTexCoord).xyz;
}