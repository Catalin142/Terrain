#version 450

layout (location = 0) in ivec2 inPosition;

void main()
{
	float y = gl_InstanceIndex / 16;
	float x = gl_InstanceIndex % 16;

	gl_Position = vec4(float(inPosition.x) +  x * 100, 0.0, float(inPosition.y) + y * 100, 1.0);
}