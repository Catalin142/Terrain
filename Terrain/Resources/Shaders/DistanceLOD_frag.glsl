#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragmentPosition;

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 2) uniform texture2D Composition;
layout (set = 1, binding = 3) uniform texture2D Normals;
layout (set = 2, binding = 0) uniform sampler2DArray texturePack;

vec3 lightDirection = normalize(vec3(10.0, 10.0, 0.0));

//#define BIPLANAR
#define TRIPLANAR

vec3 sampleTextureArray(vec3 normalVector, int layer)
{
    vec3 scaledPos = fragmentPosition * 0.25;

#ifdef TRIPLANAR
    vec3 weights = abs(normalVector);
    weights /= (weights.x + weights.y + weights.z);

    vec3 texXY = weights.z * texture(texturePack, vec3(scaledPos.xy, layer)).rgb;
    vec3 texXZ = weights.y * texture(texturePack, vec3(scaledPos.xz, layer)).rgb;
    vec3 texZY = weights.x * texture(texturePack, vec3(scaledPos.zy, layer)).rgb;

    return (texZY + texXZ + texXY);
#endif

// https://iquilezles.org/articles/biplanar/
// https://www.shadertoy.com/view/ws3Bzf
#ifdef BIPLANAR
    vec3 dpdx = dFdx(scaledPos);
    vec3 dpdy = dFdy(scaledPos);
    vec3 n = abs(normalVector);

    // major axis (in x; yz are following axis)
    ivec3 ma = (n.x > n.y && n.x > n.z) ? ivec3(0, 1, 2) :
               (n.y > n.z)              ? ivec3(1, 2, 0) :
                                          ivec3(2, 0, 1) ;
    // minor axis (in x; yz are following axis)
    ivec3 mi = (n.x < n.y && n.x < n.z) ? ivec3(0, 1, 2) :
               (n.y < n.z)              ? ivec3(1, 2, 0) :
                                          ivec3(2, 0, 1) ;
        
    // median axis (in x;  yz are following axis)
    ivec3 me = ivec3(3) - mi - ma;
    
    vec4 x = textureGrad(texturePack, vec3(scaledPos[ma.y], scaledPos[ma.z], layer), 
                                      vec2(dpdx[ma.y],dpdx[ma.z]), 
                                      vec2(dpdy[ma.y],dpdy[ma.z]));
    vec4 y = textureGrad(texturePack, vec3(scaledPos[ma.y], scaledPos[ma.z], layer), 
                                      vec2(dpdx[me.y],dpdx[me.z]),
                                      vec2(dpdy[me.y],dpdy[me.z]));

    // blend and return
    vec2 m = vec2(n[ma.x], n[me.x]);
    // optional - add local support (prevents discontinuty)
    m = clamp((m - 0.5773) / (1.0 - 0.5773), 0.0, 1.0);
	return ((x * m.x + y * m.y) / (m.x + m.y)).rgb;
#endif
}

void main() 
{
    vec4 composition = texture(sampler2D(Composition, terrainSampler), fragTexCoord);
    vec3 normal = texture(sampler2D(Normals, terrainSampler), fragTexCoord).xyz;

    float intensity = max(dot(normal, lightDirection), 0.4);

    vec3 grassTexColor = composition.x * sampleTextureArray(normal, 0);
    vec3 slopeTexColor = composition.y * sampleTextureArray(normal, 1);
    vec3 rockTexColor  = composition.z * sampleTextureArray(normal, 2);
    vec3 snowTexColor  = composition.w * vec3(1.0, 1.0, 1.0);

    vec3 finalColor = grassTexColor + slopeTexColor + rockTexColor + snowTexColor;

    // TODO add height based blending!!

    outColor = vec4(finalColor * intensity, 1.0);
    outColor.a = 1.0;
}