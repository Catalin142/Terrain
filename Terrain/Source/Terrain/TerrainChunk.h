#pragma once

#include <glm/glm.hpp>

struct TerrainChunk
{
	glm::vec2 Offset;
	uint32_t Size = 0;
	uint8_t _padding;
};