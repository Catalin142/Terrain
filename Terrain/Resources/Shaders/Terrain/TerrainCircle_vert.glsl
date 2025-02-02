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

layout (set = 1, binding = 0, r16f) uniform readonly image2DArray heightMap;

void main() 
{
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];

    vec2 offset;
    offset.x = float(chunk.Offset & 0x0000ffffu);
    offset.y = float((chunk.Offset >> 16) & 0x0000ffffu);

    int chunkSize = 128 << chunk.Lod;
    offset.x *= chunkSize;
    offset.y *= chunkSize;

    float multiplier = float(chunkSize) / 128;

    int chunkSizePlusOne = 128 + 1;
    position.x = floor(gl_VertexIndex / chunkSizePlusOne) * multiplier;
    position.z = gl_VertexIndex % chunkSizePlusOne * multiplier;
    
    position.x += offset.x;
    position.z += offset.y;
    
    gl_Position = Camera.Projection * Camera.View * position;

    if (chunk.Lod == 0)
        fragPos = vec3(1.0, 0.0, 0.0);
        
    if (chunk.Lod == 1)
        fragPos = vec3(0.0, 1.0, 0.0);
        
    if (chunk.Lod == 2)
        fragPos = vec3(0.0, 0.0, 1.0);


//    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
//    TerrainChunk chunk = Chunks.chunk[gl_InstanceIndex];
//
//    uint chunkSize = 1024 << chunk.Lod;
//
//    float snappingValue = chunkSize / terrainInfo.minimumChunkSize;
//
//    vec2 offset;
//    offset.x = int(chunk.Position.x / snappingValue / 2.0) * snappingValue * 2.0;
//    offset.y = int(chunk.Position.y / snappingValue / 2.0) * snappingValue * 2.0;
//    
//    float multiplier = float(chunkSize) / 1024;
//
//    int chunkSizePlusOne = terrainInfo.minimumChunkSize + 1;
//    position.x = floor(gl_VertexIndex / chunkSizePlusOne) * multiplier;
//    position.z = gl_VertexIndex % chunkSizePlusOne * multiplier;
//        
//    position.x += offset.x;
//    position.z += offset.y;
//    
//    terrainUV = vec2(position.x / terrainInfo.Size.x, position.z / terrainInfo.Size.y);
//    
//    vec2 camPos;
//    camPos.x = clamp(cameraInfo.Position.x, 0.0, terrainInfo.Size.x);
//    camPos.y = clamp(cameraInfo.Position.y, 0.0, terrainInfo.Size.x);
//
//    float dist = distance(position.xz, camPos);
//
//    float sinkValue = 0.0;
//    sinkValue = mix(0.0, chunkSize, 0.01 * ((chunkSize / 4.0 - dist) / (chunkSize / 4.0)) * float(chunkSize != terrainInfo.minimumChunkSize
//        && dist < chunkSize / 4.0));
//
//    ivec3 terrainLoadLayer = ivec3(uint(position.x) % 8, uint(position.z) % 8, chunk.Lod); 
//    position.y = (-imageLoad(heightMap, terrainLoadLayer).r) * terrainInfo.heightMultiplier + sinkValue;
//    
//    gl_Position = Camera.Projection * Camera.View * position;
//
//    float outsideCircle = float(dist < chunkSize / 2 || chunkSize == terrainInfo.Size.x);
//    float underGroud = float(sinkValue < chunkSize * 0.001);
//    
//    gl_Position.w *= (underGroud * outsideCircle);
//
//    fragPos = position.xyz;
//    cameraPosition = cameraInfo.Position;
}