#version 450

layout(push_constant) uniform CameraPushConstant
{
    mat4 Projection;
    mat4 View;
} Camera;

layout(location = 0) out vec3 fragPos;

layout(quads, equal_spacing, cw) in;

void main()
{
	vec4 pos1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 pos2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(pos1, pos2, gl_TessCoord.y);
	
	gl_Position = Camera.Projection * Camera.View * pos;
	fragPos = vec3(1.0, 1.0, 1.0);
}