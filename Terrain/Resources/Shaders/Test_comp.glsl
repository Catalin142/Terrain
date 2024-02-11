#version 450

layout (binding = 0, rgba32f) uniform writeonly image2D image;

layout (push_constant) uniform noiseParameters
{
    int octaves;
    float amplitude;
    float frequency;
    float gain;
    float lacunarity;
} params;

float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))*
        43758.5453123);
}

float noise (in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

float fbm (in vec2 st) {
    float value = 0.0;
    float amplitude = params.amplitude;
    float freq = params.frequency;

    for (int i = 0; i < params.octaves; i++) {
        value += amplitude * noise(freq * st);
        freq *= params.lacunarity;
        amplitude *= params.gain;
    }
    return value;
}

layout(local_size_x = 8, local_size_y = 8) in;
void main() 
{
    vec3 color = vec3(0.0);
    color += fbm((gl_GlobalInvocationID.xy / 128.0) * 3.0);

	ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
	imageStore(image, texel, vec4(color, 1.0));
}