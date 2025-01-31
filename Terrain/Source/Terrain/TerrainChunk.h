#pragma once

#include <vector>
#include <glm/glm.hpp>

struct TerrainChunk
{
	// change to size_t
	// x 16 bits, y 16 bits
	uint32_t Offset;
	uint32_t Lod = 0;

	static std::vector<uint32_t> generateIndices(uint32_t lod, uint32_t vertCount);
	static void generateIndicesAndVertices(uint32_t vertCount, std::vector<uint32_t>& indices, std::vector<glm::vec2>& vertices);
};
