#version 450

// https://gist.github.com/Leslieghf/334b8cd6ed75fb35ae334416627995ee

layout (binding = 0, r32f) uniform writeonly image2D image;
layout (binding = 1, rgba8) uniform writeonly image2D noiseTexture;

layout (push_constant) uniform noiseParameters
{
    int octaves;
    float amplitude;
    float frequency;
    float gain;
    float lacunarity;
    float b;
} params;

const int permutations[512] = int[](
  151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
  8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117,
  35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
  134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41,
  55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89,
  18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226,
  250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182,
  189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43,
  172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
  228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239,
  107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
  138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
  151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
  8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117,
  35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
  134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41,
  55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89,
  18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226,
  250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182,
  189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43,
  172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
  228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239,
  107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
  138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
);

const vec2 gradients2D[8] = vec2[](
  vec2(1.0, 0.0), vec2(-1.0, 0.0),
  vec2(0.0, 1.0), vec2(0.0, -1.0),
  vec2(0.7071067691, 0.7071067691), vec2(-0.7071067691, 0.7071067691),
  vec2(0.7071067691, -0.7071067691), vec2(-0.7071067691, -0.7071067691)
);

float smootherstep(float value) 
{
  return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
}

vec2 gradient(int x, int y) 
{
  return gradients2D[permutations[permutations[x] + y] % gradients2D.length()];
}

// Perlin noise by Ken Perlin
float cnoise(vec2 coords) 
{
  ivec2 i = ivec2(floor(coords));
  vec2 f = fract(coords);

  vec2 lb = gradient(i.x, i.y);
  vec2 rb = gradient(i.x + 1, i.y);
  vec2 lt = gradient(i.x, i.y + 1);
  vec2 rt = gradient(i.x + 1, i.y + 1);
  
  float x1  = dot(vec2(f.x, f.y), lb);
  float x2 = dot(vec2(f.x - 1.0, f.y), rb);
  float x3  = dot(vec2(f.x, f.y - 1.0), lt);
  float x4 = dot(vec2(f.x - 1.0, f.y - 1.0), rt);

  float sx = smootherstep(f.x);
  float sy = smootherstep(f.y);

  float y1 = mix(x1, x2, sx);
  float y2 = mix(x3, x4, sx);

  return mix(y1, y2, sy);
}

// https://www.shadertoy.com/view/XlGcRh
vec2 hashwithoutsine22(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

// https://iquilezles.org/articles/gradientnoise/
vec3 NoiseDerivatives(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
    
    vec2 ga = hashwithoutsine22(i + vec2(0.0, 0.0));
    vec2 gb = hashwithoutsine22(i + vec2(1.0, 0.0));
    vec2 gc = hashwithoutsine22(i + vec2(0.0, 1.0));
    vec2 gd = hashwithoutsine22(i + vec2(1.0, 1.0));
    
    float va = dot(ga, f - vec2(0.0, 0.0));
    float vb = dot(gb, f - vec2(1.0, 0.0));
    float vc = dot(gc, f - vec2(0.0, 1.0));
    float vd = dot(gd, f - vec2(1.0, 1.0));

    return vec3( va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd),   // value
                 ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +  // derivatives
                 du * (u.yx * (va - vb - vc + vd) + vec2(vb, vc) - va));
}

float StandardPerlin(vec2 point)
{
    return cnoise(point);
}

float BillowNoise(vec2 point)
{
    float noise = cnoise(point);
    return abs(noise);
}

float RidgetNoise(vec2 point)
{
    return 1.0 - BillowNoise(point);
}

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
        total += BillowNoise(coords * frequency) * amplitude;

        frequency *= params.lacunarity;
        amplitude *= 0.5;


        vec3 n = NoiseDerivatives(p);
        d += n.yz;
        total += b * n.x / (1.5 + dot(d, d));
        b *= 0.5;
        p = p * 2.0;
  }

  // Abstract
    float height = total * params.b;
    float fl = floor(height);
    float diff = height - fl;
    diff = (sigmoid2(diff * 2.0 - 1.0, params.gain) + 1.0) / 2.0;
    
    float ffloor = float(fl) / params.b;
    float fceil = float(fl + 1) / params.b;
          
    total = mix(ffloor, fceil, diff);
    
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
    float noise = domainWarping(vec2(gl_GlobalInvocationID.xy / 1024.0));
    
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
	imageStore(image, pixelCoords, vec4(noise, noise, noise, 1.0));
}