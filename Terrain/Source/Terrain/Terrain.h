#pragma once

#include "TerrainChunk.h"
#include "Graphics/Vulkan/VulkanImage.h"
#include "Graphics/Vulkan/VulkanTexture.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// 128 = 64m
struct TerrainInfo
{
	int32_t TerrainSize;
	float HeightMultiplier = 0.0f;
	int32_t ChunkSize = 128;
	uint32_t LODCount;
};

struct TerrainSpecification
{
	std::shared_ptr<VulkanImage> HeightMap;
	std::shared_ptr<VulkanImage> NormalMap;
	std::shared_ptr<VulkanImage> CompositionMap;
	std::shared_ptr<VulkanTexture> TerrainTextures;
	std::shared_ptr<VulkanTexture> NormalTextures;

	TerrainFileLocation Filepath;

	TerrainInfo Info;
};

class TerrainData
{
public:
	TerrainData(const TerrainSpecification& spec);
	~TerrainData() = default;

	TerrainInfo getInfo() { return m_Specification.Info; }
	TerrainSpecification getSpecification() { return m_Specification; }

	uint32_t getSectorCount() { return uint32_t(m_Specification.Info.TerrainSize) / m_Specification.Info.ChunkSize; }
	void setHeightMultiplier(float mul) { m_Specification.Info.HeightMultiplier = mul; }

	const std::shared_ptr<VulkanImage>& getHeightMap() { return m_Specification.HeightMap; }
	const std::shared_ptr<VulkanImage>& getNormalMap() { return m_Specification.NormalMap; }
	const std::shared_ptr<VulkanImage>& getCompositionMap() { return m_Specification.CompositionMap; }
	const std::shared_ptr<VulkanTexture>& getTerrainTextures() { return m_Specification.TerrainTextures; }
	const std::shared_ptr<VulkanTexture>& getNormalTextures() { return m_Specification.NormalTextures; }

	void addChunkProperty(size_t chunk, const FileChunkProperties& props);
	const FileChunkProperties& getChunkProperty(size_t chunk);

private:
	TerrainSpecification m_Specification;
	std::unordered_map<size_t, FileChunkProperties> m_ChunkProperties;
};