#version 460 core

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragmentPosition;

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler terrainSampler;
layout (set = 1, binding = 2) uniform texture2D Composition;
layout (set = 1, binding = 3) uniform texture2D Normals;

layout (set = 2, binding = 0) uniform sampler2DArray texturePack;
layout (set = 2, binding = 1) uniform texture2D noiseMap;

vec3 lightDirection = normalize(vec3(10.0, 10.0, 0.0));

//#define BIPLANAR
#define TRIPLANAR

float sum( vec3 v ) { return v.x+v.y+v.z; }
vec3 textureNoTile( in vec2 x, int layer)
{
    float k = texture(sampler2D(noiseMap, terrainSampler), 0.0025*x ).x; // cheap (cache friendly) lookup
    
    vec2 duvdx = dFdx( x );
    vec2 duvdy = dFdy( x );
    
    float l = k*8.0;
    float f = fract(l);
    
    float ia = floor(l+0.5); // suslik's method (see comments)
    float ib = floor(l);
    f = min(f, 1.0-f)*2.0;
    
    vec2 offa = sin(vec2(3.0,7.0)*ia); // can replace with any other hash
    vec2 offb = sin(vec2(3.0,7.0)*ib); // can replace with any other hash
    
    vec3 cola = texture(texturePack, vec3(x.xy + offa, layer)).xyz;
    vec3 colb = texture(texturePack, vec3(x.xy + offb, layer)).xyz;
    
    return mix( cola, colb, smoothstep(0.2,0.8,f-0.1*sum(cola-colb)) );
}

vec3 sampleTextureArray(vec3 normalVector, int layer)
{
    vec3 scaledPos = fragmentPosition * 0.25;

#ifdef TRIPLANAR
    vec3 weights = abs(normalVector);
    weights /= (weights.x + weights.y + weights.z);

    vec3 texXY = weights.z * textureNoTile(scaledPos.xy, layer).rgb;
    vec3 texXZ = weights.y * textureNoTile(scaledPos.xz, layer).rgb;
    vec3 texZY = weights.x * textureNoTile(scaledPos.zy, layer).rgb;

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
    
    vec3 x = textureNoTile(vec2(scaledPos[ma.y], scaledPos[ma.z]), layer);
    vec3 y = textureNoTile(vec2(scaledPos[ma.y], scaledPos[ma.z]), layer);

    // blend and return
    vec2 m = vec2(n[ma.x], n[me.x]);
    // optional - add local support (prevents discontinuty)
    //m = clamp((m - 0.5773) / (1.0 - 0.5773), 0.0, 1.0);
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