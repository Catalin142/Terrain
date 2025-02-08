#pragma once

#include <string>
#include <cstdint>

#include "Terrain/TerrainChunk.h"

struct ClipmapSpecification
{
	uint32_t ChunkSize;
	uint32_t TextureSize;
	uint32_t TerrainSize;

	uint32_t LODCount;

	TerrainFileLocation Filepath;
};

struct ClipmapLoadTask
{
	uint32_t Offset;
	uint32_t Lod;
	size_t FileOffset;
};