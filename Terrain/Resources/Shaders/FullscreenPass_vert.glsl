#version 460 core

layout(location = 0) out vec2 fragTexCoord;

const vec4 vertices[6] = {
    vec4(-1.0, -1.0, 0.0, 0.0),
    vec4( 1.0, -1.0, 1.0, 0.0),
    vec4( 1.0,  1.0, 1.0, 1.0),

    vec4(-1.0, -1.0, 0.0, 0.0),
    vec4( 1.0,  1.0, 1.0, 1.0),
    vec4(-1.0,  1.0, 0.0, 1.0)
};

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex].xy, 0.0, 1.0);
    fragTexCoord = vertices[gl_VertexIndex].zw;
}