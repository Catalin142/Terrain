#version 460 core

#define TERRIN_SIZE 1024
#define HEIGHT_MULTIPLER 250

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform CameraUniformBufferSet
{
    mat4 Projection;
    mat4 View;
} Camera;

struct TerrainChunk
{
    vec2 Offset;
    int Size;
};

layout(set = 0, binding = 1) uniform ChunksUniformBufferSet
{
    TerrainChunk chunk[256];
} Chunks;

layout(set = 0, binding = 2) uniform LodMapUniformBufferSet
{
    int lod[16 * 16];
} lodMap;

layout (set = 1, binding = 0) uniform sampler2D heightMap;

float getClosestPosition(int position, int target, int chunkSize) 
{    
    if (position == chunkSize || position == 0)
        return float(position);

    position = position + target / 2;
    position = position - (position % target);
    return float(position);
} 

int getLod(int posX, int posY)
{
    posX = clamp(posX, 0, 16);
    posY = clamp(posY, 0, 16);
    return lodMap.lod[posY * 16 + posX];
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
    
    if (position.x == chunk.Size && rightLod > currentLod)
        position.z = getClosestPosition(int(position.z), rightLod, chunk.Size);
        
    if (position.x == 0 && leftLod > currentLod)
        position.z = getClosestPosition(int(position.z), leftLod, chunk.Size);
        
    if (position.z == chunk.Size && upLod > currentLod)
        position.x = getClosestPosition(int(position.x), upLod, chunk.Size);
        
    if (position.z == 0 && downLod > currentLod)
        position.x = getClosestPosition(int(position.x), downLod, chunk.Size);
        
    texCoord = vec2(position.x / chunk.Size, position.z / chunk.Size);

    position.x += offset.x;
    position.z += offset.y;
    
    vec2 dynamicTexCoord = vec2(position.x / TERRIN_SIZE, position.z / TERRIN_SIZE);

    position.y = (1.0 - texture(heightMap, dynamicTexCoord).r) * HEIGHT_MULTIPLER;

    gl_Position = Camera.Projection * Camera.View * position;

    fragTexCoord = dynamicTexCoord;
}