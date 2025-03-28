#version 450

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

layout(location = 0) out vec3 fragPos;

layout(location = 0) patch in ivec2 p_ControlPointPosition;
layout(location = 1) patch in uint  p_Lod;
layout(location = 2) patch in ivec2 p_ChunkOffset;

layout(std430, set = 0, binding = 0) readonly buffer TessellationSettings
{
    int ControlPointSize;
    int ControlPointsPerRow;
};

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

    int multiplier = 1 << p_Lod;
    int chunkSize = terrainInfo.minimumChunkSize << p_Lod;
	
    int paddedSize = terrainInfo.minimumChunkSize + 2;

    int patchRes = ControlPointSize * ControlPointsPerRow;

    // i rescale the chunk offset to be from [0..1024 / patchRes] to be [0..1024 / terrainInfo.minimumChunkSize]
    ivec2 rescaledOffsetWrapped = ((p_ChunkOffset * patchRes / terrainInfo.minimumChunkSize) % (1024 / terrainInfo.minimumChunkSize)) * paddedSize; 

    // get the offset of the patch inside the 128 terrain patch resolution
    ivec2 CpOffset  = p_ChunkOffset % (terrainInfo.minimumChunkSize / patchRes) * patchRes;

    ivec2 wrappedOffset = rescaledOffsetWrapped + CpOffset;

	ivec2 controlPointOffset = p_ControlPointPosition * ControlPointSize;
    ivec3 terrainLoadLayer = ivec3(wrappedOffset + position.xz + controlPointOffset, p_Lod);

    float height = (imageLoad(heightMap, terrainLoadLayer).r);
	fragPos = vec3(0.0, abs(height), 0.0);

    float elevationMin = 2.0 * terrainInfo.ElevationRange.x;
    float elevationMax = 2.0 * terrainInfo.ElevationRange.y;

	height = -((height * (elevationMax - elevationMin) + elevationMin));
	position.y = height;
	
	position.xz *= multiplier;
	position.xz += controlPointOffset * multiplier;
	position.x += (p_ChunkOffset.x * (patchRes << p_Lod));
	position.z += (p_ChunkOffset.y * (patchRes << p_Lod));

	gl_Position = Camera.Projection * Camera.View * position;

}