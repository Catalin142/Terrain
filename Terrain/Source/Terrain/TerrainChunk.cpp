#include "TerrainChunk.h"


std::vector<uint32_t> TerrainChunk::generateIndices(uint32_t lod, uint32_t vertCount)
{
	std::vector<uint32_t> indices;
	for (uint32_t x = 0; x < vertCount - 1; x += lod * 2) {
		for (uint32_t y = 0; y < vertCount - 1; y += lod * 2) {
			uint32_t index = x * vertCount + y;
			/*
			c      d      i


			b      e      h


			a      f      g
			*/

			uint32_t a = index;
			uint32_t b = index + lod;
			uint32_t c = index + lod * 2;
			uint32_t d = index + vertCount * lod + lod * 2;
			uint32_t e = index + vertCount * lod + lod;
			uint32_t f = index + vertCount * lod;
			uint32_t g = index + vertCount * lod * 2;
			uint32_t h = index + vertCount * lod * 2 + lod;
			uint32_t i = index + vertCount * lod * 2 + lod * 2;

			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(e);

			indices.push_back(f);
			indices.push_back(a);
			indices.push_back(e);

			indices.push_back(b);
			indices.push_back(c);
			indices.push_back(e);

			indices.push_back(e);
			indices.push_back(c);
			indices.push_back(d);

			indices.push_back(e);
			indices.push_back(d);
			indices.push_back(i);

			indices.push_back(h);
			indices.push_back(e);
			indices.push_back(i);

			indices.push_back(f);
			indices.push_back(e);
			indices.push_back(g);

			indices.push_back(g);
			indices.push_back(e);
			indices.push_back(h);
		}
	}

	return indices;
}

void TerrainChunk::generateIndicesAndVertices(uint32_t vertCount, std::vector<uint32_t>& indices, std::vector<glm::ivec2>& vertices)
{
	for (uint32_t y = 0; y < vertCount; y++) {
		for (uint32_t x = 0; x < vertCount; x++) {
			vertices.push_back({ x, y });
		}
	}

	for (uint32_t y = 0; y < vertCount - 1; y += 2) {
		for (uint32_t x = 0; x < vertCount - 1; x += 2) {
			uint32_t index = y * vertCount + x;
			/*
			c      d      i


			b      e      h


			a      f      g
			*/

			uint32_t a = index;
			uint32_t b = index + vertCount;
			uint32_t c = index + vertCount * 2;
			uint32_t d = index + 1 + vertCount * 2;
			uint32_t e = index + 1 + vertCount;
			uint32_t f = index + 1;
			uint32_t g = index + 2;
			uint32_t h = index + 2 + vertCount;
			uint32_t i = index + 2 + vertCount * 2;

			indices.push_back(e);
			indices.push_back(f);
			indices.push_back(a);

			indices.push_back(e);
			indices.push_back(a);
			indices.push_back(b);

			indices.push_back(e);
			indices.push_back(b);
			indices.push_back(c);

			indices.push_back(e);
			indices.push_back(c);
			indices.push_back(d);

			indices.push_back(e);
			indices.push_back(d);
			indices.push_back(i);

			indices.push_back(e);
			indices.push_back(i);
			indices.push_back(h);

			indices.push_back(e);
			indices.push_back(h);
			indices.push_back(g);

			indices.push_back(e);
			indices.push_back(g);
			indices.push_back(f);
		}
	}
}