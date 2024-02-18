#pragma once

#include "TerrainChunk.h"
#include "Graphics/VulkanImage.h"
#include "Graphics/VulkanTexture.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <memory>

class DistanceLOD;

// 128 = 64m

enum class LODTechnique
{
	DISTANCE_BASED,

	NONE
};

struct LODLevel
{
	alignas(16) int32_t LOD;
};

struct TerrainInfo
{
	glm::vec2 TerrainSize{ 0.0 };
	float HeightMultiplier = 0.0f;
	uint32_t MinimumChunkSize = 128;
};

struct TerrainSpecification
{
	std::shared_ptr<VulkanImage> HeightMap;
	std::shared_ptr<VulkanImage> NormalMap;
	std::shared_ptr<VulkanImage> CompositionMap;
	std::shared_ptr<VulkanTexture> TerrainTextures;

	TerrainInfo Info;
};

class Terrain
{
public:
	Terrain(const TerrainSpecification& spec);
	~Terrain() = default;

	const std::vector<TerrainChunk>& getChunksToRender(const glm::vec3& cameraPosition);

	LODTechnique getCurrentTechnique() { return m_CurrentLODTechnique; }
	const std::shared_ptr<DistanceLOD>& getDistanceLODTechnique() { return m_DistanceLOD; }

	TerrainInfo getInfo() { return m_Specification.Info; }
	void setHeightMultiplier(float mul) { m_Specification.Info.HeightMultiplier = mul; }

	const std::shared_ptr<VulkanImage>& getHeightMap() { return m_Specification.HeightMap; }
	const std::shared_ptr<VulkanImage>& getNormalMap() { return m_Specification.NormalMap; }
	const std::shared_ptr<VulkanImage>& getCompositionMap() { return m_Specification.CompositionMap; }
	const std::shared_ptr<VulkanTexture>& getTerrainTextures() { return m_Specification.TerrainTextures; }

	std::shared_ptr<DistanceLOD> m_DistanceLOD;
private:
	TerrainSpecification m_Specification;

	LODTechnique m_CurrentLODTechnique = LODTechnique::DISTANCE_BASED;

	std::vector<TerrainChunk> m_ChunksToRender;
	std::vector<LODLevel> m_LodLevelMap;
};