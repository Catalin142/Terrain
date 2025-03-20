#version 450

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

layout(location = 0) out vec3 fragPos;

layout(location = 0) in ivec2 in_ControlPointPosition[];
layout(location = 1) in uint in_Lod[];
layout(location = 2) in ivec2 in_ChunkOffset[];

layout (set = 2, binding = 0, r16f) uniform readonly image2DArray heightMap;

layout(set = 2, binding = 1) uniform TerrainInfoUniformBuffer
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

    int multiplier = 1 << in_Lod[0];
    int chunkSize = terrainInfo.minimumChunkSize << in_Lod[0];
	
    int paddedSize = terrainInfo.minimumChunkSize + 2;
    ivec2 wrappedOffset = (in_ChunkOffset[0] % 8) * paddedSize;

	ivec2 controlPointOffset = in_ControlPointPosition[0] * 16;
    ivec3 terrainLoadLayer = ivec3(wrappedOffset + position.xz + controlPointOffset, in_Lod[0]);

    float height = (imageLoad(heightMap, terrainLoadLayer).r);
	fragPos = vec3(0.0, abs(height / 2.0), 0.0);

    float elevationMin = 2.0 * terrainInfo.ElevationRange.x;
    float elevationMax = 2.0 * terrainInfo.ElevationRange.y;

	height = -((height * (elevationMax - elevationMin) + elevationMin));
	position.y = height;
	
	position.xz *= multiplier;
	position.xz += controlPointOffset * multiplier;
	position.x += (in_ChunkOffset[0].x * (terrainInfo.minimumChunkSize << in_Lod[0]));
	position.z += (in_ChunkOffset[0].y * (terrainInfo.minimumChunkSize << in_Lod[0]));

	gl_Position = Camera.Projection * Camera.View * position;

}