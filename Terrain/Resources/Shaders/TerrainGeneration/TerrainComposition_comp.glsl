#version 450

layout (binding = 0, rgba8) uniform readonly image2D normalMap;
layout (binding = 1, r8ui) uniform writeonly uimage2D compositionMap;

// For now, need a material system with a splatmap later
const int GRASS		= 0;
const int DIRT		= 1;
const int ROCK		= 2;
const int SNOW		= 3;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

	vec4 normalInformation = imageLoad(normalMap, texel);
	vec3 normal = normalInformation.xyz;
	float height = (2.0 * normalInformation.w - 1.0);

	float slope = 1.0 - abs(normal.y);
	
	uvec4 color;

	if (height <= 0.1 && slope < 0.1)
		color.r = GRASS;
	else if (height > 0.1 && slope < 0.1)
		color.r = SNOW;
	else if (slope >= 0.2 && slope < 0.5)
		color.r = DIRT;
	else color.r = ROCK;

	imageStore(compositionMap, texel, color);
}