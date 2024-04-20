#pragma once

#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Graphics/Vulkan/VulkanTexture.h"
#include "Graphics/Vulkan/VulkanPipeline.h"
#include "Terrain/Terrain.h"

#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <vector>

struct LODRange
{
	float DistanceNear, DistanceFar;
	uint32_t LOD;
	uint32_t CurrentCount = 0;
};

class DistanceLOD
{
public:
	DistanceLOD(TerrainSpecification& spec, const std::vector<LODRange>& LODRanges);
	~DistanceLOD() = default;

	void getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition);
	const std::vector<LODRange>& getLODs() { return m_LODRanges; }

	std::vector<LODLevel> getLodMap() { return m_LodMap; }

private:
	void Initialize();

private:
	TerrainSpecification m_TerrainSpecification;

	std::vector<TerrainChunk> m_Chunks;
	std::vector<LODLevel> m_LodMap;
	std::vector<LODRange> m_LODRanges;
};
