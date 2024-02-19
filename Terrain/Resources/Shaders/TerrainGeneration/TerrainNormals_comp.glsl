#version 450

layout (binding = 0, r16f) uniform readonly image2D heightMap;
layout (binding = 1, rgba8) uniform writeonly image2D normalMap;

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

	float height = imageLoad(heightMap, texel).r;
	float b = 100.0 * imageLoad(heightMap, texel - ivec2(0, 1)).r;
	float t = 100.0 * imageLoad(heightMap, texel + ivec2(0, 1)).r;
	float r = 100.0 * imageLoad(heightMap, texel + ivec2(1, 0)).r;
	float l = 100.0 * imageLoad(heightMap, texel - ivec2(1, 0)).r;
	
    vec3 va = normalize(vec3(2.0, r - l, 0.0));
    vec3 vb = normalize(vec3(0.0, b - t, 2.0));

	vec3 normal = normalize(cross(va, vb));
	normal.y = -normal.y;
	normal.z = -normal.z;

	imageStore(normalMap, texel, vec4(normal, 1.0));
}