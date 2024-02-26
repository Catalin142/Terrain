#version 450

layout (binding = 0, rgba8) uniform readonly image2D normalMap;
layout (binding = 1, rgba8) uniform writeonly image2D compositionMap;

// For now, need a material system with a splatmap later
const vec4 GRASS	= vec4(1.0, 0.0, 0.0, 0.0);
const vec4 DIRT		= vec4(0.0, 1.0, 0.0, 0.0);
const vec4 ROCK		= vec4(0.0, 0.0, 1.0, 0.0);
const vec4 SNOW		= vec4(0.0, 0.0, 0.0, 1.0);

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

	vec4 normalInformation = imageLoad(normalMap, texel);
	vec3 normal = normalInformation.xyz;
	float height = normalInformation.w;

	float slope = (1.0 - abs(normal.y));
	
	vec4 color;
	if (slope < 0.05 && height * 100.0 < 100.0)
		color = GRASS;
	if (slope < 0.05 && height * 100.0 >= 100.0)
		color = SNOW;
		
	if (slope >= 0.05 && slope < 0.5 && height * 100.0 < 100.0)
		color = mix(GRASS, DIRT, (slope - 0.05) * 1.0 / 0.45);
	if (slope >= 0.05 && slope < 0.5 && height * 100.0 >= 100.0)
		color = mix(SNOW, DIRT, (slope - 0.05) * 1.0 / 0.45);

	if (slope >= 0.5 && slope < 0.52)
		color = DIRT;
		
	if (slope >= 0.52 && slope < 0.85)
		color = mix(DIRT, ROCK, (slope - 0.52) * 1.0 / 0.33);

	if (slope >= 0.85 && slope < 1.0)
		color = ROCK;

	imageStore(compositionMap, texel, color);
}