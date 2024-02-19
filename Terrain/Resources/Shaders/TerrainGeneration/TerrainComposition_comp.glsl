#version 450

layout (binding = 0, rgba8) uniform readonly image2D normalMap;
layout (binding = 1, rgba8) uniform writeonly image2D composition;

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

	vec3 normal = imageLoad(normalMap, texel).xyz;
	float angle = abs(1.0 - dot(normal, vec3(0.0, 1.0, 0.0)));
		
	vec4 color = vec4(1.0, 0.0, 0.0, 1.0);
	if (angle < 0.15)
		color = mix(vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), angle * (1.0 / 0.15));
	else
		color = mix(vec4(0.0, 1.0, 0.0, 1.0), vec4(0.0, 0.0, 1.0, 1.0), (angle - 0.15) * (1.0 / 0.85));

	imageStore(composition, texel, color);
}