#version 450 core

layout (vertices = 4) out;

layout(location = 0) in ivec2 in_ControlPointPosition[];
layout(location = 1) in uint in_Lod[];
layout(location = 2) in ivec2 in_ChunkOffset[];
layout(location = 3) in uint in_StitchDirection[];

layout(location = 0) patch out ivec2 p_ControlPointPosition;
layout(location = 1) patch out uint	 p_Lod;
layout(location = 2) patch out ivec2 p_ChunkOffset;

layout(std430, set = 0, binding = 0) readonly buffer TessellationSettings
{
    int ControlPointSize;
    int ControlPointsPerRow;
};

layout (set = 1, binding = 0, rgba8) uniform readonly image2DArray verticalErrorMap;

layout(std430, set = 1, binding = 1) readonly buffer ControlSpecification
{
    float LODThreshold[];
};

struct LODMargins
{
    ivec2 xMargins;
    ivec2 yMargins;
};

layout(set = 1, binding = 2) readonly buffer TerrainChunksMarginsSet
{
    LODMargins Margins[];
};

float getLod(in vec4 verticalError)
{
	float LOD = LODThreshold[in_Lod[gl_InvocationID]];
	float lod = 16.00;
	lod = mix(lod, 8.0,  step(verticalError.x, LOD));
	lod = mix(lod, 4.0,  step(verticalError.y, LOD));
	lod = mix(lod, 2.0,  step(verticalError.z, LOD));

	return lod;
}

void main(void)
{
	p_Lod = in_Lod[gl_InvocationID];
	p_ChunkOffset = in_ChunkOffset[gl_InvocationID];
	p_ControlPointPosition = in_ControlPointPosition[gl_InvocationID];

    if (gl_InvocationID == 0)
    { 
		ivec3 verticalMapSize = imageSize(verticalErrorMap);

		ivec2 wrappedOffset = in_ChunkOffset[gl_InvocationID] % (1024 / (ControlPointSize * ControlPointsPerRow));

		ivec2 cpPosition = in_ControlPointPosition[gl_InvocationID];
		uint lod = in_Lod[gl_InvocationID];

		ivec3 chunkPos = ivec3(wrappedOffset * ControlPointsPerRow + cpPosition, lod);

		vec4 verticalError = imageLoad(verticalErrorMap, chunkPos);
		
		float centerLod = getLod(verticalError);

		ivec3 rightPos	= chunkPos + ivec3(1, 0, 0);
		ivec3 leftPos	= chunkPos - ivec3(1, 0, 0);
		ivec3 upPos		= chunkPos + ivec3(0, 1, 0);
		ivec3 downPos	= chunkPos - ivec3(0, 1, 0);

		rightPos.x = ((rightPos.x % 64) + 64) % 64;
		leftPos.x  = ((leftPos.x  % 64) + 64) % 64;
		upPos.y    = ((upPos.y    % 64) + 64) % 64;
		downPos.y  = ((downPos.y  % 64) + 64) % 64;

		vec4 verticalErrorRight	= imageLoad(verticalErrorMap, rightPos);
		vec4 verticalErrorLeft	= imageLoad(verticalErrorMap, leftPos);
		vec4 verticalErrorUp	= imageLoad(verticalErrorMap, upPos);
		vec4 verticalErrorDown	= imageLoad(verticalErrorMap, downPos);
		
		float rightLod	= getLod(verticalErrorRight);
		float leftLod	= getLod(verticalErrorLeft);
		float upLod		= getLod(verticalErrorUp);
		float downLod	= getLod(verticalErrorDown);
		
		gl_TessLevelInner[0] = centerLod;
		gl_TessLevelInner[1] = centerLod;
		gl_TessLevelOuter[0] = max(centerLod, downLod);
		gl_TessLevelOuter[1] = max(centerLod, leftLod);
		gl_TessLevelOuter[2] = max(centerLod, upLod);
		gl_TessLevelOuter[3] = max(centerLod, rightLod);

		uint stitch = in_StitchDirection[gl_InvocationID];
		
		if ((stitch & 4) != 0  && cpPosition.y == 0)
			gl_TessLevelOuter[0] = 1 << lod;
		
		if ((stitch & 1) != 0 && cpPosition.x == 0)
			gl_TessLevelOuter[1] = 1 << lod;

		if ((stitch & 8) != 0  && cpPosition.y == 1)
			gl_TessLevelOuter[2] = 1 << lod;

		if ((stitch & 2) != 0 && cpPosition.x == 1)
			gl_TessLevelOuter[3] = 1 << lod;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}