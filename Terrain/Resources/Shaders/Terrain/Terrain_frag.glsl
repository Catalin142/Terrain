#version 460 core

layout(location = 0) in vec3 fragPos;
//layout(location = 1) in vec2 terrainUV;
//
layout(location = 0) out vec4 outColor;

//precision highp float;

//layout (set = 1, binding = 3, r8ui) uniform readonly uimage2D Composition;
//layout (set = 1, binding = 4) uniform texture2D normalMap;
//
//layout (set = 2, binding = 0) uniform sampler2DArray texturePack;
//layout (set = 2, binding = 1) uniform sampler2DArray normalPack;
//
//vec3 lightDirection = normalize(vec3(100.0, -50.0, 0.0));
//
//#define HEIGHT_BLEND_FACTOR 0.05
//
//// http://untitledgam.es/2017/01/height-blending-shader/
//vec3 heightBlend(vec3 input1, float height1, vec3 input2, float height2, vec3 input3, float height3, vec3 input4, float height4)
//{
//	float height_start = max(max(height1, height2), max(height3, height4)) - HEIGHT_BLEND_FACTOR;
//	float b1 = max(height1 - height_start, 0);
//	float b2 = max(height2 - height_start, 0);
//	float b3 = max(height3 - height_start, 0);
//	float b4 = max(height4 - height_start, 0);
//	return ((input1 * b1) + (input2 * b2) + (input3 * b3) + (input4 * b4)) / (b1 + b2 + b3 + b4);
//}
//
//vec2 randomRotateUV(vec2 uv, vec2 position)
//{
//    float rotation = radians(fract(sin(dot(position, vec2(12.9898, 78.233))) * 43758.5453) * 360.0);
//    float rotSin = sin(rotation);
//    float rotCos = cos(rotation);
//
//    return vec2(
//      rotCos * uv.x + rotSin * uv.y,
//      rotCos * uv.y - rotSin * uv.x
//    );
//}
//
//void getPatchAttributes(vec2 uv, vec2 ddx, vec2 ddy, ivec4 surroundingLayers, out vec3 outColor, out vec3 outNormal) 
//{
//    vec2 flooredUV = floor(fragPos.xz);
//
//    //https://advances.realtimerendering.com/s2023/Etienne(ATVI)-Large%20Scale%20Terrain%20Rendering%20with%20notes%20(Advances%202023).pdf
//    vec2 rotatedCoordTR = fract(randomRotateUV(uv, flooredUV + vec2(1.0, 1.0)));
//    vec2 rotatedCoordTL = fract(randomRotateUV(uv, flooredUV + vec2(0.0, 1.0)));
//    vec2 rotatedCoordBR = fract(randomRotateUV(uv, flooredUV + vec2(0.0, 0.0)));
//    vec2 rotatedCoordBL = fract(randomRotateUV(uv, flooredUV + vec2(1.0, 0.0)));
//    
//    vec3 coordTR = vec3(rotatedCoordTR, surroundingLayers[0]);
//    vec3 coordTL = vec3(rotatedCoordTL, surroundingLayers[1]);
//    vec3 coordBR = vec3(rotatedCoordBR, surroundingLayers[2]);
//    vec3 coordBL = vec3(rotatedCoordBL, surroundingLayers[3]);
//
//    vec3 colorTR = textureGrad(texturePack, coordTR, ddx, ddy).rgb;
//    vec3 colorTL = textureGrad(texturePack, coordTL, ddx, ddy).rgb;
//    vec3 colorBR = textureGrad(texturePack, coordBR, ddx, ddy).rgb;
//    vec3 colorBL = textureGrad(texturePack, coordBL, ddx, ddy).rgb;
//
//    vec4 normalHeightTR = textureGrad(normalPack, coordTR, ddx, ddy);
//    vec4 normalHeightTL = textureGrad(normalPack, coordTL, ddx, ddy);
//    vec4 normalHeightBR = textureGrad(normalPack, coordBR, ddx, ddy);
//    vec4 normalHeightBL = textureGrad(normalPack, coordBL, ddx, ddy);
//
//    float tTR = normalHeightTR.w * (1.0 - distance(fragPos.xz, flooredUV + vec2(1, 1)));
//    float tTL = normalHeightTL.w * (1.0 - distance(fragPos.xz, flooredUV + vec2(0, 1)));
//    float tBR = normalHeightBR.w * (1.0 - distance(fragPos.xz, flooredUV + vec2(0, 0)));
//    float tBL = normalHeightBL.w * (1.0 - distance(fragPos.xz, flooredUV + vec2(1, 0)));
//
//    vec3 finalColor = heightBlend(colorTR, tTR, colorTL, tTL, colorBR, tBR, colorBL, tBL);
//
//    vec3 finalNormal = heightBlend(normalHeightTR.xyz, tTR, 
//                                   normalHeightTL.xyz, tTL, 
//                                   normalHeightBR.xyz, tBR, 
//                                   normalHeightBL.xyz, tBL);
//
//    outNormal = finalNormal * 2.0 - 1.0;
//    outColor = finalColor;
//}
//
//ivec4 aquireMaterialIndices(ivec2 pos)
//{
//    ivec4 surroundingLayers;
//    surroundingLayers[0] = int(imageLoad(Composition, pos + ivec2(1, 1)).r);
//    surroundingLayers[1] = int(imageLoad(Composition, pos + ivec2(0, 1)).r);
//    surroundingLayers[2] = int(imageLoad(Composition, pos + ivec2(0, 0)).r);
//    surroundingLayers[3] = int(imageLoad(Composition, pos + ivec2(1, 0)).r);
//    return surroundingLayers;
//}
//
//vec3 blendNormals(vec3 worldNormal, vec3 texNormal)
//{
//    vec3 norm = vec3(0.0);
//       
//    float a = 1 / (1 + worldNormal.z);
//    float b = -worldNormal.x * worldNormal.y * a;
//    
//    vec3 b1 = vec3(1 - worldNormal.x * worldNormal.x * a, b, -worldNormal.x);
//    vec3 b2 = vec3(b, 1 - worldNormal.y * worldNormal.y * a, -worldNormal.y);
//    vec3 b3 = worldNormal;
//    
//    if (worldNormal.z < -0.9999999) // Handle the singularity
//    {
//        b1 = vec3( 0, -1, 0);
//        b2 = vec3(-1,  0, 0);
//    }
//    
//    norm = texNormal.x * b1 + texNormal.y * b2 + texNormal.z * b3;
//    return norm;
//}
//
//void triplanarMapping(ivec4 surroundingLayers, vec3 normal, out vec3 outColor, out vec3 outNormal)
//{
//	vec3 blendWeights = pow(abs(normal), vec3(3.0));
//	blendWeights /= dot(blendWeights, vec3(1.0));
//
//	const float threshold = 0.2;
//
//	vec3 finalWeights = vec3(0.0);
//
//    vec3 colorX = vec3(0.0);
//    vec3 colorY = vec3(0.0);
//    vec3 colorZ = vec3(0.0);
//
//    vec3 normalX = vec3(0.0);
//    vec3 normalY = vec3(0.0);
//    vec3 normalZ = vec3(0.0);
//    
//    vec3 ddx = dFdx(fragPos);
//    vec3 ddy = dFdy(fragPos);
//
//    if (blendWeights.x > threshold)
//    {
//        finalWeights.x = blendWeights.x;
//        getPatchAttributes(fragPos.yz, ddx.yz, ddy.yz, surroundingLayers, colorX, normalX);
//    }
//    
//    if (blendWeights.y > threshold)
//    {
//        finalWeights.y = blendWeights.y;
//        getPatchAttributes(fragPos.xz, ddx.xz, ddy.xz, surroundingLayers, colorY, normalY);
//    }
//     
//    if (blendWeights.z > threshold)
//    {
//        finalWeights.z = blendWeights.z;
//        getPatchAttributes(fragPos.xy, ddx.xy, ddy.xy, surroundingLayers, colorZ, normalZ);
//    }
//    
//	finalWeights /= finalWeights.x + finalWeights.y + finalWeights.z;
//
//    // https://blog.selfshadow.com/publications/blending-in-detail/
//    outNormal = normalX * finalWeights.x + normalY * finalWeights.y + normalZ * finalWeights.z;
//    outNormal = blendNormals(normal, outNormal);
//
//    outColor  = colorX * finalWeights.x + colorY * finalWeights.y + colorZ * finalWeights.z;
//}

void main() 
{
//    vec3 normal = vec3(0.0, 1.0, 0.0);
//
//    ivec4 surroundingLayers = aquireMaterialIndices(ivec2(fragPos.x, fragPos.z));
//    
//    vec3 finalColor = vec3(0.0);
//    vec3 finalNormal = vec3(0.0);
//    triplanarMapping(surroundingLayers, normal, finalColor, finalNormal);
//
//    float intensity = max(dot(finalNormal, -lightDirection), 0.2);
    
    outColor = vec4(fragPos, 1.0);
}