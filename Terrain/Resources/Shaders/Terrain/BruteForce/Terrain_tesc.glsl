#version 450 core

layout (vertices = 4) out;

layout(location = 0) in ivec2 in_ChunkOffset[];
layout(location = 1) in uint in_Lod[];
layout(location = 2) in uint in_Neightbours[];

layout(location = 0) out ivec2 out_ChunkOffset[];
layout(location = 1) out uint out_Lod[];

layout(set = 1, binding = 1) uniform TerrainInfoUniformBuffer
{
    int Size;
    vec2 ElevationRange;
    int minimumChunkSize;
    uint LODCount;
} terrainInfo;

void main(void)
{
	out_ChunkOffset[gl_InvocationID] = in_ChunkOffset[gl_InvocationID];
	out_Lod[gl_InvocationID] = in_Lod[gl_InvocationID];

    if (gl_InvocationID == 0)
    { 
		int divisor = 1 << in_Lod[gl_InvocationID];
		int centerLod = 128 / divisor;
		
		uint neightbours = in_Neightbours[gl_InvocationID];

		int upDivisor      = 1 << (int(neightbours & 0xff000000u) >> 24);
		int downDivisor    = 1 << (int(neightbours & 0x00ff0000u) >> 16);
		int leftDivisor    = 1 << (int(neightbours & 0x0000ff00u) >> 8);
		int rightDivisor   = 1 << (int(neightbours & 0x000000ffu));

		gl_TessLevelInner[0] = centerLod;
		gl_TessLevelInner[1] = centerLod;
		gl_TessLevelOuter[0] = max(centerLod, terrainInfo.minimumChunkSize / downDivisor);
		gl_TessLevelOuter[1] = max(centerLod, terrainInfo.minimumChunkSize / leftDivisor);
		gl_TessLevelOuter[2] = max(centerLod, terrainInfo.minimumChunkSize / upDivisor);
		gl_TessLevelOuter[3] = max(centerLod, terrainInfo.minimumChunkSize / rightDivisor);
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}