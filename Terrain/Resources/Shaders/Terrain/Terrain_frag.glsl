#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragmentPosition;
layout(location = 3) flat in int go;
layout(location = 4) flat in ivec2 quadPosition;

layout(location = 0) out vec4 outColor;

precision highp float;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 2, r8ui) uniform readonly uimage2D Composition;
layout (set = 1, binding = 3) uniform texture2D Normals;

layout (set = 2, binding = 0) uniform sampler2DArray texturePack;
layout (set = 2, binding = 1) uniform sampler2DArray normalPack;
layout (set = 2, binding = 2) uniform texture2D noiseMap;

vec3 lightDirection = normalize(vec3(10.0, 10.0, 0.0));

//#define BIPLANAR
//#define TRIPLANAR

//float sum( vec3 v ) { return v.x+v.y+v.z; }
//vec3 textureNoTile( in vec2 x, int layer)
//{
//    float k = texture(sampler2D(noiseMap, terrainSampler), 0.0025*x ).x; // cheap (cache friendly) lookup
//    
//    vec2 duvdx = dFdx( x );
//    vec2 duvdy = dFdy( x );
//    
//    float l = k*8.0;
//    float f = fract(l);
//    
//    float ia = floor(l+0.5); // suslik's method (see comments)
//    float ib = floor(l);
//    f = min(f, 1.0-f)*2.0;
//    
//    vec2 offa = sin(vec2(3.0,7.0)*ia); // can replace with any other hash
//    vec2 offb = sin(vec2(3.0,7.0)*ib); // can replace with any other hash
//    
//    vec3 cola = texture(texturePack, vec3(x.xy + offa, layer)).xyz;
//    vec3 colb = texture(texturePack, vec3(x.xy + offb, layer)).xyz;
//    
//    return mix( cola, colb, smoothstep(0.2,0.8,f-0.1*sum(cola-colb)) );
//}
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
vec3 heightblend(vec3 input1, float height1, vec3 input2, float height2, vec3 input3, float height3, vec3 input4, float height4)
{
	float height_start = max(max(height1, height2), max(height3, height4)) - HEIGHT_BLEND_FACTOR;
	float b1 = max(height1 - height_start, 0);
	float b2 = max(height2 - height_start, 0);
	float b3 = max(height3 - height_start, 0);
	float b4 = max(height4 - height_start, 0);
	return ((input1 * b1) + (input2 * b2) + (input3 * b3) + (input4 * b4)) / (b1 + b2 + b3 + b4);
}

float hash1(uint value) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return (float(value % 360));
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

float checkers( in vec2 p )
{
    vec2 q = floor(p);
    return mod(q.x + q.y, 15.);
}

vec2 randomRotateUV(vec2 uv, vec2 position)
{
    return rotateUV(uv, radians(rand(position) * 360.0), vec2(0.0, 0.0));
}

vec4 sampleRotatedUV(sampler2DArray tex, vec2 uv, int layer, vec2 ddx, vec2 ddy)
{
     return textureGrad(tex, vec3(fract(uv), layer), ddx, ddy);
}

vec3 getFinalColor() 
{
    ivec2 intPos = ivec2(fragmentPosition.x, fragmentPosition.z);
    vec2 flooredUV = floor(fragmentPosition.xz);

    int layerTR = int(imageLoad(Composition, intPos + ivec2(1, 1)).r);
    int layerTL = int(imageLoad(Composition, intPos + ivec2(0, 1)).r);
    int layerBR = int(imageLoad(Composition, intPos + ivec2(0, 0)).r);
    int layerBL = int(imageLoad(Composition, intPos + ivec2(1, 0)).r);

    vec2 ddx = dFdx(fragmentPosition.xz);
    vec2 ddy = dFdy(fragmentPosition.xz);

    //https://advances.realtimerendering.com/s2023/Etienne(ATVI)-Large%20Scale%20Terrain%20Rendering%20with%20notes%20(Advances%202023).pdf
    vec2 rotatedCoordTR = randomRotateUV(texCoord, flooredUV + vec2(1.0, 1.0));
    vec2 rotatedCoordTL = randomRotateUV(texCoord, flooredUV + vec2(0.0, 1.0));
    vec2 rotatedCoordBR = randomRotateUV(texCoord, flooredUV + vec2(0.0, 0.0));
    vec2 rotatedCoordBL = randomRotateUV(texCoord, flooredUV + vec2(1.0, 0.0));
    
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

    vec3 finalColor = heightblend(colorTR, tTR, colorTL, tTL, colorBR, tBR, colorBL, tBL);
    
    vec3 normal = texture(sampler2D(Normals, terrainSampler), fragTexCoord).xyz;

    vec3 tangentX = normalize(cross(normal, vec3(0.0, -1.0, 0.0)));
    vec3 bitangentX = normalize(cross(tangentX, normal));
    mat3 tbnX = mat3(tangentX, bitangentX, normal);

    vec3 finalNormal = heightblend(normalHeightTR.xyz * tbnX, tTR, 
                                   normalHeightTL.xyz * tbnX, tTL, 
                                   normalHeightBR.xyz * tbnX, tBR, 
                                   normalHeightBL.xyz * tbnX, tBL);

    float intensity = max(dot(normalHeightTR.xyz * tbnX, lightDirection) * 2.0, 0.4);

    return finalColor * intensity;
}

void main() 
{
    vec3 finalColor = getFinalColor();

    //vec3 sampColor = heightblend(vec3(fract(rotatedCoord1.xy), 0.0), height1 * t1, 
    //vec3(fract(rotatedCoord2.xy), 0.0), height2 * t2, 
    //vec3(fract(rotatedCoord3.xy), 0.0), height3 * t3, 
    //vec3(fract(rotatedCoord4.xy), 0.0), height4 * t4);

    outColor = vec4(finalColor, 1.0);
}