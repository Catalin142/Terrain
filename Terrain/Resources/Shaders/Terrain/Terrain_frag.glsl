#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragmentPosition;

layout(location = 0) out vec4 outColor;

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
    return float(value % 360);
}

uint hash(uint value) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value % 15;
}

void main() 
{
    ivec2 fPos = ivec2(fragmentPosition.x, fragmentPosition.z);

    int layerTR = int(imageLoad(Composition, fPos + ivec2(1, 1)).r);
    int layerTL = int(imageLoad(Composition, fPos + ivec2(0, 1)).r);
    int layerBR = int(imageLoad(Composition, fPos + ivec2(0, 0)).r);
    int layerBL = int(imageLoad(Composition, fPos + ivec2(1, 0)).r);

    float rotations[15] = {radians(73.9), radians(47.6), radians(33.9), radians(91.7), radians(156.9), radians(189.7), 
    radians(211.1), radians(253.2), radians(198.3), radians(311.9), radians(169.2), radians(122.7), radians(237.2), radians(133.3), radians(69.3)};

    //vec4 composition = texture(sampler2D(Composition, terrainSampler), fragTexCoord);
    vec3 normal = texture(sampler2D(Normals, terrainSampler), fragTexCoord).xyz;

    //https://advances.realtimerendering.com/s2023/Etienne(ATVI)-Large%20Scale%20Terrain%20Rendering%20with%20notes%20(Advances%202023).pdf
    //uint rotation1 = ( ( int ( ( fPos.x + 1 ) * 0.25 ) % 2 ) + 3 * ( ( int ( ( fPos.y + 1 ) * 0.25 ) % 2 ) ) % 4 ) & 0xF;
    //uint rotation1 = ( ( int ( ( fPos.x + 1 ) * 0.5 ) ) + 3 * ( ( int ( ( fPos.y + 1 ) * 0.5 )  )  ) ) ;
    //uint rotation1 = ( ( ( 7 *  int ( ( fPos.x + 1 ) * 0.5 ) % 4  ) +  3 * ( ( int ( ( fPos.y + 1) * 0.5 ) % 4  )  ) ) ) ;
    uint rotation1 = int((fPos.x + 1)* 0.5) * 17 + int((fPos.y + 1)* 0.5);
   // uint rotation1 = (( int (( fPos.x + 1 ) * 0.25 ) % 2 ) + 3 * ( int (( fPos.y + 1 ) * 0.25 ) % 2 ) % 4 ) & 15;
    //mat2 rot1 = mat2(cos((rotations[hash(rotation1)])),  sin((rotations[hash(rotation1)])),
    //                -sin((rotations[hash(rotation1)])),  cos((rotations[hash(rotation1)])));

    mat2 rot1 = mat2(cos((hash1(rotation1))),  sin((hash1(rotation1))),
                    -sin((hash1(rotation1))),  cos((hash1(rotation1))));

    vec2 rotatedCoord1 = texCoord * rot1;
    
    //uint rotation2 = ( ( int ( ( fPos.x ) * 0.25 ) % 2 ) + 3 * ( ( int ( ( fPos.y + 1 ) * 0.25 ) % 2 ) ) % 4 ) & 0xF;
    //uint rotation2 = ( ( ( 7 *  int ( ( fPos.x ) * 0.5 )   ) +  ( ( int ( ( fPos.y + 1) * 0.5 )   )  ) ) ) & 15;
    uint rotation2 = int((fPos.x) * 0.5)* 17 + int((fPos.y + 1)* 0.5);
    mat2 rot2 = mat2(cos((hash1(rotation2))),  sin((hash1(rotation2))),
                    -sin((hash1(rotation2))),  cos((hash1(rotation2))));

    vec2 rotatedCoord2 = texCoord * rot2;
    
    //uint rotation3 = ( ( int ( ( fPos.x ) * 0.25 ) % 2 ) + 3 * ( ( int ( ( fPos.y ) * 0.25 ) % 2 ) ) % 4 ) & 0xF;
    //uint rotation3 = ( ( ( 7 *  int ( ( fPos.x ) * 0.5 )   ) +  ( ( int ( ( fPos.y ) * 0.5 )   )  ) ) ) & 15;
    uint rotation3 = int((fPos.x)* 0.5 )* 17 + int((fPos.y)* 0.5);
    mat2 rot3 = mat2(cos((hash1(rotation3))),  sin((hash1(rotation3))),
                    -sin((hash1(rotation3))),  cos((hash1(rotation3))));

    vec2 rotatedCoord3 = texCoord * rot3;
    
    //uint rotation4 = ( ( int ( ( fPos.x + 1 ) * 0.25 ) % 2 ) + 3 * ( ( int ( ( fPos.y ) * 0.25 ) % 2 ) ) % 4 ) & 0xF;
    //uint rotation4 = ( ( ( 7 *  int ( ( fPos.x + 1 ) * 0.5 )   ) +  ( ( int ( ( fPos.y ) * 0.5 )  )  ) ) ) & 15;
    uint rotation4 = int((fPos.x + 1) * 0.5 )* 17 + int((fPos.y)* 0.5);
    mat2 rot4 = mat2(cos((hash1(rotation4))),  sin((hash1(rotation4))),
                    -sin((hash1(rotation4))),  cos((hash1(rotation4))));

    vec2 rotatedCoord4 = texCoord * rot4;

    vec3 color1 = texture(texturePack, vec3(rotatedCoord1.xy, layerTR)).rgb;
    vec3 color2 = texture(texturePack, vec3(rotatedCoord2.xy, layerTL)).rgb;
    vec3 color3 = texture(texturePack, vec3(rotatedCoord3.xy, layerBR)).rgb;
    vec3 color4 = texture(texturePack, vec3(rotatedCoord4.xy, layerBL)).rgb;

    float height1 = texture(normalPack, vec3(rotatedCoord1.xy, layerTR)).a;
    float height2 = texture(normalPack, vec3(rotatedCoord2.xy, layerTL)).a;
    float height3 = texture(normalPack, vec3(rotatedCoord3.xy, layerBR)).a;
    float height4 = texture(normalPack, vec3(rotatedCoord4.xy, layerBL)).a;

    //vec3 normals = texture(normalPack, vec3(texCoord.xy, layer)).rgb;
    //vec3 normals1 = texture(normalPack, vec3(texCoord.xy, layerup)).rgb;
    //vec3 normals2 = texture(normalPack, vec3(texCoord.xy, layerdown)).rgb;
    //vec3 normals3 = texture(normalPack, vec3(texCoord.xy, layerleft)).rgb;
    //vec3 normals4 = texture(normalPack, vec3(texCoord.xy, layerright)).rgb;
    
    //vec3 axis = sign(normal);
    //vec3 colorn = texture(normalPack, vec3(texCoord.xy, layer)).xyz;
    //vec3 tangent = normalize(cross(colorn, normal));
    //vec3 bitangent = normalize(cross(tangent, colorn)) * normal;
    //mat3 tbn = mat3(tangent, bitangent, colorn);


    //vec3 color1n = normal * texture(normalPack, vec3(texCoord.xy, layerup)).rgb;
    //vec3 color2n = normal * texture(normalPack, vec3(texCoord.xy, layerdown)).rgb;
    //vec3 color3n = normal * texture(normalPack, vec3(texCoord.xy, layerleft)).rgb;
    //vec3 color4n = normal * texture(normalPack, vec3(texCoord.xy, layerright)).rgb;

    //vec3 slopeTexColor = composition.y * sampleTextureArray(normal, 1);
    //vec3 rockTexColor  = composition.z * sampleTextureArray(normal, 2);
    //vec3 snowTexColor  = composition.w * vec3(1.0, 1.0, 1.0);
    
    // OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE OPTIMIZE !!! 

    //colorn = tbn * blendTerrain(normal3s, heights);

    //vec3 finalColor = (color + color1 + color2 + color3 + color4) / 5;
    //vec3 normal = colorn;

    float outheight = 0;
    
    float t1 = clamp(1.0 - distance(fragmentPosition.xz, vec2(fPos) + vec2(1, 1)), 0.0, 1.0);
    float t2 = clamp(1.0 - distance(fragmentPosition.xz, vec2(fPos) + vec2(0, 1)), 0.0, 1.0);
    float t3 = clamp(1.0 - distance(fragmentPosition.xz, vec2(fPos) + vec2(0, 0)), 0.0, 1.0);
    float t4 = clamp(1.0 - distance(fragmentPosition.xz, vec2(fPos) + vec2(1, 0)), 0.0, 1.0);

    vec3 finalColor = heightblend(color1, height1 * t1, color2, height2 * t2, color3, height3 * t3, color4, height4 * t4);

    float intensity = max(dot(normal, lightDirection) * 2.0, 0.4);

    // TODO add height based blending!!

    //outColor = vec4(vec3(float(hash(rotation1)) / 15.0, float(hash(rotation1)) / 15.0, float(hash(rotation1)) / 15.0) * intensity, 1.0);
    outColor = vec4(finalColor * intensity, 1.0);
    outColor.a = 1.0;
}