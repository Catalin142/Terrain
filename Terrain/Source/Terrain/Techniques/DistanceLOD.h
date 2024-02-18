#pragma once

#include "Graphics/VulkanBuffer.h"
#include "Graphics/VulkanTexture.h"
#include "Graphics/VulkanPipeline.h"
#include "Terrain/Terrain.h"

#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <vector>

struct LODProperties
{
	uint32_t IndicesCount = 0;
	std::shared_ptr<VulkanBuffer> IndexBuffer;
	uint32_t CurrentCount = 0;
};

struct LODRange
{
	float DistanceNear, DistanceFar;
	uint32_t LOD;
};

class DistanceLOD
{
public:
	DistanceLOD(TerrainSpecification& spec, const std::vector<LODRange>& LODRanges);
	~DistanceLOD() = default;

	void getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition);
	const std::vector<LODProperties>& getLODProperties() { return m_LODProperties; }

	std::vector<LODLevel> getLodMap() { return m_LodMap; }

private:
	void Initialize();

	std::vector<uint32_t> generateIndices(uint32_t lod, uint32_t vertCount);

private:
	TerrainSpecification& m_TerrainSpecification;
	std::vector<LODProperties> m_LODProperties;

	std::vector<TerrainChunk> m_Chunks;
	std::vector<LODLevel> m_LodMap;
	std::vector<LODRange> m_LODRanges;
};
