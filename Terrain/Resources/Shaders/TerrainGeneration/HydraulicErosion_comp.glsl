#version 450

// https://www.firespark.de/resources/downloads/implementation%20of%20a%20methode%20for%20hydraulic%20erosion.pdf

layout (binding = 0, r16f) uniform image2D heigthMap;

layout (push_constant) uniform erosionParameters
{
    int simulations;

	float inertia;

	float erosionSpeed;
	float depositSpeed;
	float evaporation;

	float initalWater;
	float intialSpeed;

	float maxCapacity;

} params;

layout(set = 1, binding = 0) uniform Seed
{
	float x;
	float y;
} seed;

struct PositionInfo
{
	vec4 heights;
	vec3 gradientXYposition;
};

bool outsideOfMap(vec2 position)
{
	float mapSize = float(imageSize(heigthMap));
	return position.x < 0.0 || position.x >= mapSize || position.y < 0.0 || position.y >= mapSize;
}

PositionInfo getGradientAndHeight(vec2 position)
{
	vec2 cellOffset = fract(position);
	ivec2 cellGridPosition = ivec2(position);

	float heightTL = imageLoad(heigthMap, cellGridPosition).x;
	float heightTR = float(outsideOfMap(cellGridPosition + vec2(1, 0)) == false) * imageLoad(heigthMap, cellGridPosition + ivec2(1, 0)).x;
	float heightBL = float(outsideOfMap(cellGridPosition + vec2(0, 1)) == false) * imageLoad(heigthMap, cellGridPosition + ivec2(0, 1)).x;
	float heightBR = float(outsideOfMap(cellGridPosition + vec2(1, 1)) == false) * imageLoad(heigthMap, cellGridPosition + ivec2(1, 1)).x;

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
	vec2 dopletPosition = vec2(random((gl_GlobalInvocationID.x + 1) * seed.x), random((gl_GlobalInvocationID.x + 1) * seed.y)) * (1024.0 * 4.0);

	vec2 velocity = vec2(0.0);

    float water = params.initalWater;
    float sediment = 0.0;
	float speed = params.intialSpeed;

	for (int sim = 0; sim < params.simulations; sim++)
	{
		PositionInfo ghOld = getGradientAndHeight(dopletPosition);

		velocity = velocity * params.inertia - ghOld.gradientXYposition.xy * (1.0 - params.inertia);

		float len = max(0.01, sqrt(velocity.x * velocity.x + velocity.y * velocity.y));
        velocity.x /= len;
        velocity.y /= len;

		vec2 newDopletPosition = dopletPosition + velocity;
		
		if ((velocity.x == 0.0 && velocity.y == 0.0) || outsideOfMap(newDopletPosition))
			break;

		PositionInfo ghNew = getGradientAndHeight(newDopletPosition);

		float hDiff = ghNew.gradientXYposition.z - ghOld.gradientXYposition.z;

		float capacity = max(-hDiff, 0.01) * speed * water * params.maxCapacity;
		
		barrier();
		groupMemoryBarrier();

		// It went uphill
		if (sediment > capacity || hDiff > 0.0)
		{
			float amountToDeposit = (hDiff > 0) ? min(hDiff, sediment) : (sediment - capacity) * params.depositSpeed;
            sediment -= amountToDeposit;

			vec2 off = fract(dopletPosition);
			imageStore(heigthMap, ivec2(dopletPosition), vec4(ghOld.heights[0] + amountToDeposit * (1.0 - off.x) * (1.0 - off.y)));
			imageStore(heigthMap, ivec2(dopletPosition) + ivec2(1, 0), vec4(ghOld.heights[1] + amountToDeposit * off.x * (1.0 - off.y)));
			imageStore(heigthMap, ivec2(dopletPosition) + ivec2(0, 1), vec4(ghOld.heights[2] + amountToDeposit * (1.0 - off.x) * off.y));
			imageStore(heigthMap, ivec2(dopletPosition) + ivec2(1, 1), vec4(ghOld.heights[3] + amountToDeposit * off.x * off.y));
		}
		else
		{
            float amountToErode = min((capacity - sediment) * params.erosionSpeed, -hDiff);
			
			vec2 off = fract(dopletPosition);

            sediment += amountToErode;

			imageStore(heigthMap, ivec2(dopletPosition), vec4(ghOld.heights[0] - amountToErode * (1.0 - off.x) * (1.0 - off.y)));
			imageStore(heigthMap, ivec2(dopletPosition) + ivec2(1, 0), vec4(ghOld.heights[1] - amountToErode * off.x * (1.0 - off.y)));
			imageStore(heigthMap, ivec2(dopletPosition) + ivec2(0, 1), vec4(ghOld.heights[2] - amountToErode * (1.0 - off.x) * off.y));
			imageStore(heigthMap, ivec2(dopletPosition) + ivec2(1, 1), vec4(ghOld.heights[3] - amountToErode * off.x * off.y));
		}
		
		barrier();
		groupMemoryBarrier();

		water *= (1.0 - params.evaporation);
        speed = sqrt(max(0, speed * speed + hDiff * 4.0));

		dopletPosition = newDopletPosition;
	}

	//imageStore(heigthMap, texel, vec4(abs(val), 0.0, 0.0, 0.0));
}
