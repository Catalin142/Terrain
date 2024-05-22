#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragmentPosition;
layout(location = 3) flat in int go;
layout(location = 4) flat in ivec2 quadPosition;
layout(location = 5) in vec3 normal;

layout(location = 0) out vec4 outColor;

precision highp float;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 2, r8ui) uniform readonly uimage2D Composition;

layout (set = 2, binding = 0) uniform sampler2DArray texturePack;
layout (set = 2, binding = 1) uniform sampler2DArray normalPack;
layout (set = 2, binding = 2) uniform texture2D noiseMap;

vec3 lightDirection = normalize(vec3(100.0, 50.0, 0.0));

//#define BIPLANAR
//#define TRIPLANAR

//float sum( vec3 v ) { return v.x+v.y+v.z; }
//
//vec3 sampleTextureArray(vec3 normalVector, int layer)
//{
//    vec3 scaledPos = fragmentPosition * (1.0 / 2.0);
//
//#ifdef TRIPLANAR
//    vec3 weights = abs(normalVector);
//    weights /= (weights.x + weights.y + weights.z);
//
//    vec3 texXY = weights.z * textureNoTile(scaledPos.xy, layer).rgb;
//    vec3 texXZ = weights.y * textureNoTile(scaledPos.xz, layer).rgb;
//    vec3 texZY = weights.x * textureNoTile(scaledPos.zy, layer).rgb;
//
//    return (texZY + texXZ + texXY);
//#endif
//
//// https://iquilezles.org/articles/biplanar/
//// https://www.shadertoy.com/view/ws3Bzf
//#ifdef BIPLANAR
//    vec3 dpdx = dFdx(scaledPos);
//    vec3 dpdy = dFdy(scaledPos);
//    vec3 n = abs(normalVector);
//
//    // major axis (in x; yz are following axis)
//    ivec3 ma = (n.x > n.y && n.x > n.z) ? ivec3(0, 1, 2) :
//               (n.y > n.z)              ? ivec3(1, 2, 0) :
//                                          ivec3(2, 0, 1) ;
//    // minor axis (in x; yz are following axis)
//    ivec3 mi = (n.x < n.y && n.x < n.z) ? ivec3(0, 1, 2) :
//               (n.y < n.z)              ? ivec3(1, 2, 0) :
//                                          ivec3(2, 0, 1) ;
//        
//    // median axis (in x;  yz are following axis)
//    ivec3 me = ivec3(3) - mi - ma;
//    
//    vec3 x =  texture(texturePack, vec3(vec2(scaledPos[ma.y], scaledPos[ma.z]), layer)).rgb;
//    vec3 y =  texture(texturePack, vec3(vec2(scaledPos[ma.y], scaledPos[ma.z]), layer)).rgb;
//
//    // blend and return
//    vec2 m = vec2(n[ma.x], n[me.x]);
//    // optional - add local support (prevents discontinuty)
//    //m = clamp((m - 0.5773) / (1.0 - 0.5773), 0.0, 1.0);
//	return ((x * m.x + y * m.y) / (m.x + m.y)).rgb;
//#endif
//}

#define HEIGHT_BLEND_FACTOR 0.05

// http://untitledgam.es/2017/01/height-blending-shader/
vec3 heightBlend(vec3 input1, float height1, vec3 input2, float height2, vec3 input3, float height3, vec3 input4, float height4)
{
	float height_start = max(max(height1, height2), max(height3, height4)) - HEIGHT_BLEND_FACTOR;
	float b1 = max(height1 - height_start, 0);
	float b2 = max(height2 - height_start, 0);
	float b3 = max(height3 - height_start, 0);
	float b4 = max(height4 - height_start, 0);
	return ((input1 * b1) + (input2 * b2) + (input3 * b3) + (input4 * b4)) / (b1 + b2 + b3 + b4);
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 rotateUV(vec2 uv, float rotation, vec2 mid)
{
    return vec2(
      cos(rotation) * (uv.x - mid.x) + sin(rotation) * (uv.y - mid.y) + mid.x,
      cos(rotation) * (uv.y - mid.y) - sin(rotation) * (uv.x - mid.x) + mid.y
    );
}

vec2 randomRotateUV(vec2 uv, vec2 position)
{
    return rotateUV(uv, radians(rand(position) * 360.0), vec2(0.0, 0.0));
}

vec4 sampleRotatedUV(sampler2DArray tex, vec2 uv, int layer, vec2 ddx, vec2 ddy)
{
     return textureGrad(tex, vec3(uv, layer), ddx, ddy);
}

int layerTR;
int layerTL;
int layerBR;
int layerBL;
void getFinalColor(vec2 uv, vec3 tangent, vec3 bitangent, vec2 ddx, vec2 ddy, out vec3 outColor, out vec3 outNormal) 
{
    vec2 flooredUV = floor(fragmentPosition.xz);

    //https://advances.realtimerendering.com/s2023/Etienne(ATVI)-Large%20Scale%20Terrain%20Rendering%20with%20notes%20(Advances%202023).pdf
    vec2 rotatedCoordTR = randomRotateUV(uv, flooredUV + vec2(1.0, 1.0));
    vec2 rotatedCoordTL = randomRotateUV(uv, flooredUV + vec2(0.0, 1.0));
    vec2 rotatedCoordBR = randomRotateUV(uv, flooredUV + vec2(0.0, 0.0));
    vec2 rotatedCoordBL = randomRotateUV(uv, flooredUV + vec2(1.0, 0.0));
    
    vec3 colorTR = sampleRotatedUV(texturePack, rotatedCoordTR, layerTR, ddx, ddy).rgb;
    vec3 colorTL = sampleRotatedUV(texturePack, rotatedCoordTL, layerTL, ddx, ddy).rgb;
    vec3 colorBR = sampleRotatedUV(texturePack, rotatedCoordBR, layerBR, ddx, ddy).rgb;
    vec3 colorBL = sampleRotatedUV(texturePack, rotatedCoordBL, layerBL, ddx, ddy).rgb;

    vec4 normalHeightTR = sampleRotatedUV(normalPack, rotatedCoordTR, layerTR, ddx, ddy);
    vec4 normalHeightTL = sampleRotatedUV(normalPack, rotatedCoordTL, layerTL, ddx, ddy);
    vec4 normalHeightBR = sampleRotatedUV(normalPack, rotatedCoordBR, layerBR, ddx, ddy);
    vec4 normalHeightBL = sampleRotatedUV(normalPack, rotatedCoordBL, layerBL, ddx, ddy);

    float tTR = normalHeightTR.w * (1.0 - distance(fragmentPosition.xz, flooredUV + vec2(1, 1)));
    float tTL = normalHeightTL.w * (1.0 - distance(fragmentPosition.xz, flooredUV + vec2(0, 1)));
    float tBR = normalHeightBR.w * (1.0 - distance(fragmentPosition.xz, flooredUV + vec2(0, 0)));
    float tBL = normalHeightBL.w * (1.0 - distance(fragmentPosition.xz, flooredUV + vec2(1, 0)));

    vec3 finalColor = heightBlend(colorTR, tTR, colorTL, tTL, colorBR, tBR, colorBL, tBL);

    mat3 tbn = mat3(tangent, bitangent, normal);

    vec3 finalNormal = heightBlend(normalHeightTR.xyz, tTR, 
                                   normalHeightTL.xyz, tTL, 
                                   normalHeightBR.xyz, tBR, 
                                   normalHeightBL.xyz, tBL);

    outNormal = finalNormal * 2.0 - 1.0;
    outColor = finalColor;
}

// Gollent_Marcin_Landscape_Creation.pdf
#define TIGHTEN_TRIPLANR_MAPPING_FACTOR 0.3

void main() 
{

    ivec2 intPos = ivec2(fragmentPosition.x, fragmentPosition.z);
    layerTR = int(imageLoad(Composition, intPos + ivec2(1, 1)).r);
    layerTL = int(imageLoad(Composition, intPos + ivec2(0, 1)).r);
    layerBR = int(imageLoad(Composition, intPos + ivec2(0, 0)).r);
    layerBL = int(imageLoad(Composition, intPos + ivec2(1, 0)).r);
    
    vec3 absNormal = abs(normal);
    absNormal /= (absNormal.x + absNormal.y + absNormal.z);
    vec3 weights = absNormal - TIGHTEN_TRIPLANR_MAPPING_FACTOR;

    vec3 finalColorX = vec3(0.0);
    vec3 finalColorY = vec3(0.0);
    vec3 finalColorZ = vec3(0.0);

    vec3 finalNormalX = vec3(0.0);
    vec3 finalNormalY = vec3(0.0);
    vec3 finalNormalZ = vec3(0.0);

    vec3 axis = sign(normal);
    vec3 finalWeight = vec3(0.0);
    
    vec3 ddx = dFdx(fragmentPosition);
    vec3 ddy = dFdy(fragmentPosition);

    if (weights.x > 0.0)
    {
        vec3 tangent = normalize(cross(normal, vec3(0.0, axis.x, 0.0)));
        vec3 bitangent = normalize(cross(tangent, normal)) * axis.x;
        getFinalColor(fragmentPosition.yz, tangent, bitangent, ddx.yz, ddy.yz, finalColorX, finalNormalX);
        finalWeight.x = absNormal.x;
    }
    
    if (weights.y > 0.0)
    {
        vec3 tangent = normalize(cross(normal, vec3(0.0, 0.0, axis.y)));
        vec3 bitangent = normalize(cross(tangent, normal)) * axis.y;
        getFinalColor(fragmentPosition.xz, tangent, bitangent, ddx.xz, ddy.xz, finalColorY, finalNormalY);
        finalWeight.y = absNormal.y;
   }
    
    if (weights.z > 0.0)
    {
        vec3 tangent = normalize(cross(normal, vec3(0.0, -axis.z, 0.0)));
        vec3 bitangent = normalize(-cross(tangent, normal)) * axis.z;
        getFinalColor(fragmentPosition.xy, tangent, bitangent, ddx.xy, ddy.xy, finalColorZ, finalNormalZ);
        finalWeight.z = absNormal.z;
    }

    
	vec3 blendWeights = pow(abs(normal), vec3(3.0));
	blendWeights /= dot(blendWeights, vec3(1.0));

	const float threshold = 0.3;

	vec3 finalWeights = vec3(0.0);
	if(blendWeights.x > threshold) finalWeights.x = blendWeights.x;
	if(blendWeights.y > threshold) finalWeights.y = blendWeights.y;
	if(blendWeights.z > threshold) finalWeights.z = blendWeights.z;

	finalWeights /= finalWeights.x + finalWeights.y + finalWeights.z;

    finalWeight /= (finalWeight.x + finalWeight.y + finalWeight.z);
    weights /= (weights.x + weights.y + weights.z);
    
    vec3 normalX = vec3(0.0, finalNormalX.yx);
    vec3 normalY = vec3(finalNormalY.x, 0.0, finalNormalY.y);
    vec3 normalZ = vec3(finalNormalZ.xy, 0.0);

    vec3 combinedNormal = finalNormalX * finalWeights.x + finalNormalY * finalWeights.y + finalNormalZ * finalWeights.z + normal;

    float intensity = max(dot(combinedNormal, lightDirection), 0.5);

    vec3 combinedColor = finalColorX * finalWeights.x + finalColorY * finalWeights.y + finalColorZ * finalWeights.z;
    outColor = vec4((combinedNormal * 0.5 + 0.5), 1.0);
}