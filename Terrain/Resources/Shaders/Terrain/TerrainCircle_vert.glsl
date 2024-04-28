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

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 1) uniform texture2D heightMap;

void main() 
{
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];

    float snappingValue = chunk.Size / terrainInfo.minimumChunkSize;
    vec2 offset;
    offset.x = int(chunk.Offset.x / snappingValue / 2.0) * snappingValue * 2.0;
    offset.y = int(chunk.Offset.y / snappingValue / 2.0) * snappingValue * 2.0;
    
    float multiplier = float(chunk.Size) / terrainInfo.minimumChunkSize;

    int chunkSizePlusOne = terrainInfo.minimumChunkSize + 1;
    position.x = floor(gl_VertexIndex / chunkSizePlusOne) * multiplier;
    position.z = gl_VertexIndex % chunkSizePlusOne * multiplier;
        
    texCoord = vec2(position.x / terrainInfo.minimumChunkSize, position.z / terrainInfo.minimumChunkSize);
    
    position.x += offset.x;
    position.z += offset.y;

    vec2 dynamicTexCoord = vec2(position.x / terrainInfo.Size.x, position.z / terrainInfo.Size.y);
    
    vec2 camPos;
    camPos.x = clamp(cameraInfo.Position.x, 0.0, terrainInfo.Size.x);
    camPos.y = clamp(cameraInfo.Position.y, 0.0, terrainInfo.Size.x);

    float dist = distance(position.xz, camPos);

    float sinkValue = 0.0;
    sinkValue = mix(0.0, chunk.Size, 0.01 * ((chunk.Size / 4.0 - dist) / (chunk.Size / 4.0)) * float(chunk.Size != terrainInfo.minimumChunkSize
        && dist < chunk.Size / 4.0));

    position.y = (-texture(sampler2D(heightMap, terrainSampler), dynamicTexCoord).r) * terrainInfo.heightMultiplier + sinkValue;
    
    gl_Position = Camera.Projection * Camera.View * position;

    float outsideCircle = float(dist < chunk.Size / 2 || chunk.Size == terrainInfo.Size.x);
    float underGroud = float(sinkValue < chunk.Size * 0.001);
    
    gl_Position.w *= (underGroud * outsideCircle);

    fragmentPosition = position.xyz;

    fragTexCoord = dynamicTexCoord;
}