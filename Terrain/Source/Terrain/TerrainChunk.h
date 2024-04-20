#pragma once

#include <vector>
#include <glm/glm.hpp>

struct TerrainChunk
{
	glm::vec2 Offset;
	uint32_t Size = 0;
	uint32_t Lod = 0;

	static std::vector<uint32_t>  generateIndices(uint32_t lod, uint32_t vertCount);
};