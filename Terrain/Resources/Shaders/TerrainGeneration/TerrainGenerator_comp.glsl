#version 450

#include "Resources/Shaders/Include/Noise_comp.glsl"

// https://gist.github.com/Leslieghf/334b8cd6ed75fb35ae334416627995ee

layout (binding = 0, r16f) uniform writeonly image2D heightMap;

layout (push_constant) uniform noiseParameters
{
    int octaves;
    float amplitude;
    float frequency;
    float lacunarity;
    vec2 offset;
} params;


float sigmoid2(float x, float sharpness) {
    if (x >= 1.0) return 1.0;
    else if (x <= -1.0) return -1.0;
    else 
    {
        if (sharpness < 0.0) sharpness -= 1.0;

        if (x > 0.0) return sharpness * x / (sharpness - x + 1.0);
        else if (x < 0.0) return sharpness * x / (sharpness - abs(x) + 1.0);
        else return 0.0;
    }
}

float computeFbm(vec2 coords, int octaveCount) 
{
    float frequency = params.frequency;
    float amplitude = params.amplitude;
    float total = 0.0;

    vec2 d = vec2(0.0);
    vec2 p = coords;
    float a = 0.0;
    float b = 1.0;

    for (int i = 0; i < octaveCount; ++i) 
    {
        total += PerlinNoise(coords * frequency) * amplitude;

        frequency *= params.lacunarity;
        amplitude *= 0.5;

        //vec3 n = NoiseDerivatives(p);
        //d += n.yz;
        //total += b * n.x / (1.5 + dot(d, d));
        //b *= 0.5;
        //p = p * 2.0;
  }

  // Abstract
   //float height = total * params.b;
   //float fl = floor(height);
   //float diff = height - fl;
   //diff = (sigmoid2(diff * 2.0 - 1.0, params.gain) + 1.0) / 2.0;
   //
   //float ffloor = float(fl) / params.b;
   //float fceil = float(fl + 1) / params.b;
   //      
   //total = mix(ffloor, fceil, diff);
    
    return total;
}

float domainWarping(vec2 point)
{
    float x = computeFbm(point + vec2(0.0, 0.0), params.octaves);
    float y = computeFbm(point + vec2(5.2, 1.6), params.octaves);

    vec2 q = vec2(x, y);
    return computeFbm(point + 4.0 * q, params.octaves);
}

layout(local_size_x = 8, local_size_y = 8) in;
void main() 
{
    vec2 position = gl_GlobalInvocationID.xy + params.offset;
    float noise = computeFbm(vec2(position / 1024.0), params.octaves);
    
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
	imageStore(heightMap, pixelCoords, vec4(noise, noise, noise, 1.0));
}