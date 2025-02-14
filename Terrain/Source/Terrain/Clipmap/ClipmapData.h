#pragma once

#include <string>
#include <cstdint>
#include "glm/glm.hpp"

#include "Terrain/TerrainChunk.h"

struct ClipmapTerrainSpecification
{
	uint32_t ClipmapSize;
};

struct ClipmapLoadTask
{
	size_t CameraHash;
	FileChunkProperties ChunkProperties;
};

struct LODMargins
{
	glm::ivec2 xMargins;
	glm::ivec2 yMargins;
};
