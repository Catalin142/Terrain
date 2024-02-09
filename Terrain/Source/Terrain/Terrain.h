#pragma once

#include "TerrainChunk.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <memory>

class DistanceLOD;

enum class LODTechnique
{
	DISTANCE_BASED,

	NONE
};

struct LODLevel
{
	alignas(16) int32_t LOD;
};

struct TerrainSpecification
{
	std::string HeightMap;
	std::string CompositionMap;
	std::vector<std::string> TerrainTextures;

	uint32_t MinimumChunkSize = 128;

	glm::vec2 TerrainSize{ 0 };
};

class Terrain
{
public:
	Terrain(const TerrainSpecification& spec);
	~Terrain() = default;

	const std::vector<TerrainChunk>& getChunksToRender(const glm::vec3& cameraPosition);

	LODTechnique getCurrentTechnique() { return m_CurrentLODTechnique; }
	const std::shared_ptr<DistanceLOD>& getDistanceLODTechnique() { return m_DistanceLOD; }

private:
	TerrainSpecification m_Specification;

	LODTechnique m_CurrentLODTechnique = LODTechnique::DISTANCE_BASED;
	std::shared_ptr<DistanceLOD> m_DistanceLOD;

	std::vector<TerrainChunk> m_ChunksToRender;
	std::vector<LODLevel> m_LodLevelMap;
};