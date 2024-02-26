#version 450

// https://www.firespark.de/resources/downloads/implementation%20of%20a%20methode%20for%20hydraulic%20erosion.pdf

layout (binding = 0, r32f) uniform image2D normalMap;

layout(set = 1, binding = 0) uniform Seed
{
	float x;
	float y;
} seed;

#define SIMULATIONS 300

struct PositionInfo
{
	vec4 heights;
	vec3 gradientXYposition;
};

bool outsideOfMap(vec2 position)
{
	float mapSize = float(imageSize(normalMap));
	return position.x < 0.0 || position.x >= mapSize || position.y < 0.0 || position.y >= mapSize;
}

PositionInfo getGradientAndHeight(vec2 position)
{
	vec2 cellOffset = fract(position);
	ivec2 cellGridPosition = ivec2(position);

	float heightTL = imageLoad(normalMap, cellGridPosition).x;
	float heightTR = float(outsideOfMap(cellGridPosition + vec2(1, 0)) == false) * imageLoad(normalMap, cellGridPosition + ivec2(1, 0)).x;
	float heightBL = float(outsideOfMap(cellGridPosition + vec2(0, 1)) == false) * imageLoad(normalMap, cellGridPosition + ivec2(0, 1)).x;
	float heightBR = float(outsideOfMap(cellGridPosition + vec2(1, 1)) == false) * imageLoad(normalMap, cellGridPosition + ivec2(1, 1)).x;

	vec2 gradient;
	gradient.x = (heightTR - heightTL) * (1.0 - cellOffset.y) + (heightBR - heightBL) * cellOffset.y;
	gradient.y = (heightBL - heightTL) * (1.0 - cellOffset.x) + (heightBR - heightTR) * cellOffset.x;

	// Based on how much each corner contributes to the height
	float height = 
		heightTL * (1.0 - cellOffset.x) * (1.0 - cellOffset.y) + 
		heightTR * cellOffset.x * (1.0 - cellOffset.y) + 
		heightBL * (1.0 - cellOffset.x) * cellOffset.y + 
		heightBR * cellOffset.x * cellOffset.y;

	PositionInfo result;
	result.heights = vec4(heightTL, heightTR, heightBL, heightBR);
	result.gradientXYposition = vec3(gradient, height);

	return result;
}

uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu;
    const uint ieeeOne      = 0x3F800000u;

    m &= ieeeMantissa;                    
    m |= ieeeOne;                         

    float  f = uintBitsToFloat( m );      
    return f - 1.0;                       
}

float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }

layout(local_size_x = 1024, local_size_y = 1) in;
void main()
{
	//vec2 dopletPosition = p.pos[gl_LocalInvocationID.x];
	vec2 dopletPosition = vec2(random((gl_GlobalInvocationID.x + 1) * seed.x), random((gl_GlobalInvocationID.x + 1) * seed.y)) * 1024.0;

	vec2 velocity = vec2(0.0);

	const float intertia = 0.3;
	
    float water = 1.0;
    float sediment = 0.0;
	float speed = 1.0;

	for (int sim = 0; sim < SIMULATIONS; sim++)
	{
		PositionInfo ghOld = getGradientAndHeight(dopletPosition);

		velocity = velocity * intertia - ghOld.gradientXYposition.xy * (1.0 - intertia);

		float len = max(0.01, sqrt(velocity.x * velocity.x + velocity.y * velocity.y));
        velocity.x /= len;
        velocity.y /= len;

		if ((velocity.x == 0.0 && velocity.y == 0.0) || outsideOfMap(dopletPosition))
			break;

		vec2 newDopletPosition = dopletPosition + velocity;
		
		if ((velocity.x == 0.0 && velocity.y == 0.0) || outsideOfMap(newDopletPosition))
			break;

		PositionInfo ghNew = getGradientAndHeight(newDopletPosition);

		float hDiff = ghNew.gradientXYposition.z - ghOld.gradientXYposition.z;

		// 0.1 = minSlope
		// 0.3 = speed
		// 0.2 = capacity
        // float capacity = max(-hDiff * 1.0 * water * 3.0 * speed, 0.01);
		float capacity = max(-hDiff, 0.01) * speed * water * 3.0;
		
		barrier();
		groupMemoryBarrier();

		// It went uphill
		if (sediment > capacity || hDiff > 0.0)
		{
			float amountToDeposit = (hDiff > 0) ? min(hDiff, sediment) : (sediment - capacity) * 0.3;
            sediment -= amountToDeposit;

			vec2 off = fract(dopletPosition);
			imageStore(normalMap, ivec2(dopletPosition), vec4(ghOld.heights[0] + amountToDeposit * (1.0 - off.x) * (1.0 - off.y)));
			imageStore(normalMap, ivec2(dopletPosition) + ivec2(1, 0), vec4(ghOld.heights[1] + amountToDeposit * off.x * (1.0 - off.y)));
			imageStore(normalMap, ivec2(dopletPosition) + ivec2(0, 1), vec4(ghOld.heights[2] + amountToDeposit * (1.0 - off.x) * off.y));
			imageStore(normalMap, ivec2(dopletPosition) + ivec2(1, 1), vec4(ghOld.heights[3] + amountToDeposit * off.x * off.y));
		}
		else
		{
            float amountToErode = min((capacity - sediment) * 0.3, -hDiff);
			
			vec2 off = fract(dopletPosition);

            sediment += amountToErode;

			imageStore(normalMap, ivec2(dopletPosition), vec4(ghOld.heights[0] - amountToErode * (1.0 - off.x) * (1.0 - off.y)));
			imageStore(normalMap, ivec2(dopletPosition) + ivec2(1, 0), vec4(ghOld.heights[1] - amountToErode * off.x * (1.0 - off.y)));
			imageStore(normalMap, ivec2(dopletPosition) + ivec2(0, 1), vec4(ghOld.heights[2] - amountToErode * (1.0 - off.x) * off.y));
			imageStore(normalMap, ivec2(dopletPosition) + ivec2(1, 1), vec4(ghOld.heights[3] - amountToErode * off.x * off.y));
		}
		
		barrier();
		groupMemoryBarrier();

		water *= 0.99;
        speed = sqrt(max(0, speed * speed + hDiff * 4.0));

		dopletPosition = newDopletPosition;
	}

	//imageStore(normalMap, texel, vec4(abs(val), 0.0, 0.0, 0.0));
}
