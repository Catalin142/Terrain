#version 450

layout (binding = 0, r16f) uniform readonly image2D heightMap;
layout (binding = 1, rgba8_snorm) uniform writeonly image2D normalMap;

float getHeight(ivec2 position)
{
	return 100.0 * imageLoad(heightMap, position).r;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

	float height = imageLoad(heightMap, texel).r;

	float b = getHeight(texel - ivec2(0, 1));
	float t = getHeight(texel + ivec2(0, 1));
	float r = getHeight(texel + ivec2(1, 0));
	float l = getHeight(texel - ivec2(1, 0));

    vec3 va = normalize(vec3(2.0, r - l, 0.0));
    vec3 vb = normalize(vec3(0.0, b - t, 2.0));

	vec3 normal = normalize(cross(va, vb));
	normal.y = -normal.y;
	normal.z = -normal.z;

	imageStore(normalMap, texel, vec4(normal, (0.5 * (height / 5.0) + 0.5)));
}