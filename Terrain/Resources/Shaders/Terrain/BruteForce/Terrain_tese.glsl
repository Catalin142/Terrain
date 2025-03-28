#version 450

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

layout(location = 0) out vec3 fragPos;

layout(location = 0) in ivec2 in_ChunkOffset[];
layout(location = 1) in uint in_Lod[];

layout (set = 1, binding = 0, r16) uniform readonly image2D heightMap;

layout(set = 1, binding = 1) uniform TerrainInfoUniformBuffer
{
    int Size;
    vec2 ElevationRange;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

layout(quads, equal_spacing, cw) in;
void main()
{
	vec4 pos1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 pos2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 position = mix(pos1, pos2, gl_TessCoord.y);

    int chunkSize = terrainInfo.minimumChunkSize;
    ivec2 offset = in_ChunkOffset[0] * chunkSize;

    ivec2 terrainLoadLayer = ivec2(offset + position.xz);

    float height = imageLoad(heightMap, terrainLoadLayer).r;
	
    float elevationMin = 2.0 * terrainInfo.ElevationRange.x;
    float elevationMax = 2.0 * terrainInfo.ElevationRange.y;

	height = -((height * (elevationMax - elevationMin) + elevationMin));
    position.y = height;

	position.xz += offset;

	gl_Position = Camera.Projection * Camera.View * position;
    
	fragPos = vec3(0.0, abs(height), 0.0);
}