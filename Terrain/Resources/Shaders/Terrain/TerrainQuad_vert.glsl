#version 460 core

#extension GL_EXT_debug_printf : enable

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
};

layout(set = 0, binding = 0) readonly buffer ChunksStorageBufferSet
{
    TerrainChunk chunk[];
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
layout (set = 1, binding = 1) uniform texture2D heightMapPhysicalTexture;
layout (set = 1, binding = 2, r32ui) uniform readonly uimage2D indirectionTexture[6];

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

    vec2 offset;
    offset.x = float(chunk.Offset & 0x0000ffffu);
    offset.y = float((chunk.Offset >> 16) & 0x0000ffffu);

    int chunkSize = terrainInfo.minimumChunkSize << chunk.Lod;
    offset.x *= chunkSize;
    offset.y *= chunkSize;

    float multiplier = float(chunkSize) / terrainInfo.minimumChunkSize;

    int chunkSizePlusOne = terrainInfo.minimumChunkSize + 1;
    position.x = floor(gl_VertexIndex / chunkSizePlusOne) * multiplier;
    position.z = gl_VertexIndex % chunkSizePlusOne * multiplier;

    ivec2 chunkPosition = ivec2((offset.x) / terrainInfo.minimumChunkSize, (offset.y) / terrainInfo.minimumChunkSize);

    //int chunkFarOffset = chunkSize / terrainInfo.minimumChunkSize;
    //int currentLod = int(chunk.Lod);
    //int upLod      = getLod(chunkPosition.x, chunkPosition.y + chunkFarOffset);
    //int downLod    = getLod(chunkPosition.x, chunkPosition.y - 1);
    //int rightLod   = getLod(chunkPosition.x + chunkFarOffset, chunkPosition.y);
    //int leftLod    = getLod(chunkPosition.x - 1, chunkPosition.y);
    //
    //// stiching patches
    //float rl, ll, ul, dl;
    //rl = (rightLod - float(int(position.z) % rightLod)) * float(position.x == chunkSize) * float((int(position.z) % rightLod) != 0);
    //ll = (leftLod - float(int(position.z) % leftLod)) * float(position.x == 0.0) * float((int(position.z) % leftLod) != 0);
    //ul = (upLod - float(int(position.x) % upLod)) * float(position.z == chunkSize) * float((int(position.x) % upLod) != 0);
    //dl = (downLod - float(int(position.x) % downLod)) * float(position.z == 0.0) * float((int(position.x) % downLod) != 0);
    //
    //position.z += rl;
    //position.z += ll;
    //position.x += ul;
    //position.x += dl;
    
    vec2 terrainPhLoc = vec2(position.x, position.z);

    //if (rl != 0.0 || ll != 0.0)
    //    terrainPhLoc.y = position.z - 0.5;
    //if (ul != 0.0 || dl != 0.0)
    //    terrainPhLoc.x = position.x - 0.5;
        
    position.x += offset.x;
    position.z += offset.y;
    
    terrainUV = vec2(position.x / terrainInfo.Size.x, position.z / terrainInfo.Size.y);
    
    uint chunkPhysicalLocation;
    
    switch (chunk.Lod) 
    {
        case 0: chunkPhysicalLocation = imageLoad(indirectionTexture[0], chunkPosition / int(multiplier)).r; break;
        case 1: chunkPhysicalLocation = imageLoad(indirectionTexture[1], chunkPosition / int(multiplier)).r; break;
        case 2: chunkPhysicalLocation = imageLoad(indirectionTexture[2], chunkPosition / int(multiplier)).r; break;
        case 3: chunkPhysicalLocation = imageLoad(indirectionTexture[3], chunkPosition / int(multiplier)).r; break;
        case 4: chunkPhysicalLocation = imageLoad(indirectionTexture[4], chunkPosition / int(multiplier)).r; break;
        case 5: chunkPhysicalLocation = imageLoad(indirectionTexture[5], chunkPosition / int(multiplier)).r; break;
    }

    // TODO: fix hardcoded values
    int chunkPhyX = 130 * int(chunkPhysicalLocation & 0x0000ffffu);
    int chunkPhyY = 130 * int((chunkPhysicalLocation >> 16) & 0x0000ffffu);
    
    terrainPhLoc /= float(1 << chunk.Lod);

    terrainPhLoc.x += float(chunkPhyX + 1);
    terrainPhLoc.y += float(chunkPhyY + 1);

    cameraPosition = vec2(chunkPhyX, chunkPhysicalLocation);
    terrainPhLoc /= (1040.0);

    position.y = (-texture(sampler2D(heightMapPhysicalTexture, terrainSampler), terrainPhLoc).r) * terrainInfo.heightMultiplier;
    //position.y = terrainPhLoc.y * terrainInfo.heightMultiplier;

    gl_Position = Camera.Projection * Camera.View * position;

    fragPos = position.xyz;
}