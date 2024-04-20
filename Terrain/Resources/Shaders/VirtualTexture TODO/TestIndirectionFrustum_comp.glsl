#version 450

layout (binding = 0, r32ui) uniform writeonly uimage2D indirectionTexture[7];
layout (binding = 1, rgba8) uniform writeonly image2D visualization;

layout (push_constant) uniform Camera
{
    vec2 position;
	vec2 forward;
	vec2 right;
	float fov;

} camera;


struct Point
{
	int x, y;
};

float sign(Point p1, Point p2, Point p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool PointInTriangle(Point pt, Point v1, Point v2, Point v3)
{
    float d1, d2, d3;
    bool has_neg, has_pos;

    d1 = sign(pt, v1, v2);
    d2 = sign(pt, v2, v3);
    d3 = sign(pt, v3, v1);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

const float aspect = 1600.0 / 900.0;

layout(local_size_x = 10, local_size_y = 10) in;
void main()
{
	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
	imageStore(indirectionTexture[0], texel, uvec4(gl_GlobalInvocationID.x / 90.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	imageStore(indirectionTexture[1], texel, uvec4(gl_GlobalInvocationID.x / 80.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	imageStore(indirectionTexture[2], texel, uvec4(gl_GlobalInvocationID.x / 60.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	imageStore(indirectionTexture[3], texel, uvec4(gl_GlobalInvocationID.x / 40.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	imageStore(indirectionTexture[4], texel, uvec4(gl_GlobalInvocationID.x / 30.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	imageStore(indirectionTexture[5], texel, uvec4(gl_GlobalInvocationID.x / 20.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	imageStore(indirectionTexture[6], texel, uvec4(gl_GlobalInvocationID.x / 10.0, gl_GlobalInvocationID.y / 100.0, 0.0, 1.0));
	
    const float halfVSide = 50.0 * tan(radians(camera.fov * .5 * aspect));
	
    vec2 frontMultFar = camera.position + 50.0 * camera.forward - camera.right * halfVSide;
    vec2 frontMultFar2 = camera.position + 50.0 * camera.forward + camera.right * halfVSide;

	Point current;
	current.x = texel.x;
	current.y = texel.y;

	Point origin;
	origin.x = int(camera.position.x);
	origin.y = int(camera.position.y);

	Point left;
	left.x = int(frontMultFar.x);
	left.y = int(frontMultFar.y);

	Point right;
	right.x = int(frontMultFar2.x);
	right.y = int(frontMultFar2.y);

	vec3 color = vec3(0.0, 0.0, 0.0);
	if (PointInTriangle(current, left, origin, right) == true)
		color = vec3(1.0, 0.0, 0.0);
		
	imageStore(visualization, texel, vec4(color, 1.0));
}