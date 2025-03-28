#version 460
#extension GL_EXT_shader_atomic_float  : enable

layout (set = 0, binding = 0, r16f) uniform readonly image2DArray heightMap;
layout (set = 0, binding = 1, rgba16f) uniform writeonly image2DArray verticalErrorMap;

precision highp float;

shared vec4 maxVerticalError;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main()
{
	maxVerticalError = vec4(0.0);

	memoryBarrierShared();
	barrier();

	uint LOD = gl_GlobalInvocationID.z;

	ivec2 texelChunk = ivec2(gl_GlobalInvocationID.x / 16, gl_GlobalInvocationID.y / 16);
	ivec2 chunkOffset = texelChunk / 8;
	chunkOffset *= 130;
	chunkOffset += 1;

	ivec2 texelPosition = chunkOffset + (texelChunk % 8) * 16 + ivec2(gl_LocalInvocationID.x, gl_LocalInvocationID.y);

	ivec3 heightMapSize = imageSize(heightMap) - ivec3(1, 1, 0);

	float pixelHeight = imageLoad(heightMap, ivec3(texelPosition, LOD)).x;

	vec3 verticalError = vec3(0.0);

	// lod 8x8
	{
		ivec3 texelPosition8x8 = ivec3(texelPosition.x / 2 * 2, texelPosition.y / 2 * 2, LOD);

		ivec3 BLPos8x8 = min(texelPosition8x8, heightMapSize);
		ivec3 BRPos8x8 = min(texelPosition8x8 + ivec3(2, 0, 0), heightMapSize);
		ivec3 TLPos8x8 = min(texelPosition8x8 + ivec3(0, 2, 0), heightMapSize);
		ivec3 TRPos8x8 = min(texelPosition8x8 + ivec3(2, 2, 0), heightMapSize);

		float BL8x8 = imageLoad(heightMap, BLPos8x8).x;
		float BR8x8 = imageLoad(heightMap, BRPos8x8).x;
		float TL8x8 = imageLoad(heightMap, TLPos8x8).x;
		float TR8x8 = imageLoad(heightMap, TRPos8x8).x;

		float distX = float(texelPosition.x - BLPos8x8.x) / 2.0;
		float distY = float(texelPosition.y - TRPos8x8.y) / 2.0;
	
		float xBMix = mix(BL8x8, BR8x8, distX);
		float xTMix = mix(TL8x8, TR8x8, distX);
		verticalError.x = abs(pixelHeight - mix(xBMix, xTMix, distY));
	}

	// lod 4x4
	{
		ivec3 texelPosition4x4 = ivec3(texelPosition.x / 4 * 4, texelPosition.y / 4 * 4, LOD);

		ivec3 BLPos4x4 = min(texelPosition4x4, heightMapSize);
		ivec3 BRPos4x4 = min(texelPosition4x4 + ivec3(4, 0, 0), heightMapSize);
		ivec3 TLPos4x4 = min(texelPosition4x4 + ivec3(0, 4, 0), heightMapSize);
		ivec3 TRPos4x4 = min(texelPosition4x4 + ivec3(4, 4, 0), heightMapSize);

		float BL4x4 = imageLoad(heightMap, BLPos4x4).x;
		float BR4x4 = imageLoad(heightMap, BRPos4x4).x;
		float TL4x4 = imageLoad(heightMap, TLPos4x4).x;
		float TR4x4 = imageLoad(heightMap, TRPos4x4).x;

		float distX = float(texelPosition.x - BLPos4x4.x) / 4.0;
		float distY = float(texelPosition.y - TRPos4x4.y) / 4.0;
	
		float xBMix = mix(BL4x4, BR4x4, distX);
		float xTMix = mix(TL4x4, TR4x4, distX);
		verticalError.y = abs(pixelHeight - mix(xBMix, xTMix, distY));
	}	
	
	// lod 2x2
	{
		ivec3 texelPosition2x2 = ivec3(texelPosition.x / 8 * 8, texelPosition.y / 8 * 8, LOD);

		ivec3 BLPos2x2 = min(texelPosition2x2, heightMapSize);
		ivec3 BRPos2x2 = min(texelPosition2x2 + ivec3(8, 0, 0), heightMapSize);
		ivec3 TLPos2x2 = min(texelPosition2x2 + ivec3(0, 8, 0), heightMapSize);
		ivec3 TRPos2x2 = min(texelPosition2x2 + ivec3(8, 8, 0), heightMapSize);

		float BL2x2 = imageLoad(heightMap, BLPos2x2).x;
		float BR2x2 = imageLoad(heightMap, BRPos2x2).x;
		float TL2x2 = imageLoad(heightMap, TLPos2x2).x;
		float TR2x2 = imageLoad(heightMap, TRPos2x2).x;
	
		float distX = float(texelPosition.x - BLPos2x2.x) / 8.0;
		float distY = float(texelPosition.y - TRPos2x2.y) / 8.0;
	
		float xBMix = mix(BL2x2, BR2x2, distX);
		float xTMix = mix(TL2x2, TR2x2, distX);
		verticalError.z = abs(pixelHeight - mix(xBMix, xTMix, distY));
	}
	
	atomicAdd(maxVerticalError.x, verticalError.x);
	atomicAdd(maxVerticalError.y, verticalError.y);
	atomicAdd(maxVerticalError.z, verticalError.z);
	maxVerticalError.w = 1.0;

	memoryBarrierShared();
	barrier();

	if (gl_LocalInvocationID.x * gl_LocalInvocationID.y == 1)
	{
		imageStore(verticalErrorMap, ivec3(texelChunk, LOD), maxVerticalError);
	}
}