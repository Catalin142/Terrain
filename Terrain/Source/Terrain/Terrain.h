#pragma once

#include "TerrainChunk.h"
#include "VirtualMap/TerrainVirtualMap.h"
#include "Graphics/Vulkan/VulkanImage.h"
#include "Graphics/Vulkan/VulkanTexture.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <memory>

// 128 = 64m

struct LODLevel
{
	alignas(16) int32_t LOD;
};

struct TerrainInfo
{
	glm::vec2 TerrainSize{ 0.0f };
	float HeightMultiplier = 0.0f;
	uint32_t MinimumChunkSize = 128;
	uint32_t LodCount;
};

struct TerrainSpecification
{
	std::shared_ptr<VulkanImage> HeightMap;
	std::shared_ptr<VulkanImage> NormalMap;
	std::shared_ptr<VulkanImage> CompositionMap;
	std::shared_ptr<VulkanTexture> TerrainTextures;
	std::shared_ptr<VulkanTexture> NormalTextures;

	TerrainInfo Info;
};

class Terrain
{
public:
	Terrain(const TerrainSpecification& spec);
	~Terrain() = default;

	TerrainInfo getInfo() { return m_Specification.Info; }
	TerrainSpecification getSpecification() { return m_Specification; }

	uint32_t getSectorCount() { return uint32_t(m_Specification.Info.TerrainSize.x) / m_Specification.Info.MinimumChunkSize; }
	void setHeightMultiplier(float mul) { m_Specification.Info.HeightMultiplier = mul; }

	const std::shared_ptr<VulkanImage>& getHeightMap() { return m_Specification.HeightMap; }
	const std::shared_ptr<VulkanImage>& getNormalMap() { return m_Specification.NormalMap; }
	const std::shared_ptr<VulkanImage>& getCompositionMap() { return m_Specification.CompositionMap; }
	const std::shared_ptr<VulkanTexture>& getTerrainTextures() { return m_Specification.TerrainTextures; }
	const std::shared_ptr<VulkanTexture>& getNormalTextures() { return m_Specification.NormalTextures; }

private:
	TerrainSpecification m_Specification;
};