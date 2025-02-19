#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/VirtualMap/TerrainVirtualMap.h"

#include "Graphics/Vulkan/VulkanComputePipeline.h"
#include "Graphics/Vulkan/VulkanBuffer.h"

#include <vector>
#include <memory>

// Technique highly inspired by Far Cry 5: https://www.gdcvault.com/play/1025261/Terrain-Rendering-in-Far-Cry
struct QuadTreePassMetadata
{
	uint32_t TMPArray1Index;
	uint32_t ResultArrayIndex;
	uint32_t DataLoaded;
};

// Everything happens on the GPU, highly performant
// Works only with virtual maps
class QuadTreeLOD
{
public:
	QuadTreeLOD(const TerrainSpecification& spec, const std::shared_ptr<TerrainVirtualMap>& virtualMap);
	~QuadTreeLOD() = default;

	void Generate(VkCommandBuffer commandBuffer, std::vector<TerrainChunk> firstPass, uint32_t bufferIndex);

private:
	void createResources(const std::shared_ptr<TerrainVirtualMap>& virtualMap);

public:
	std::shared_ptr<VulkanBuffer> ChunksToRender;
	std::shared_ptr<VulkanBufferSet> PassMetadata;

private:
	TerrainSpecification m_TerrainSpecification;

	std::shared_ptr<VulkanComputePipeline> m_ConstructQuadTreePipeline;
	std::vector<std::shared_ptr<VulkanDescriptorSet>> m_DescriptorSets;
	
	std::shared_ptr<VulkanBufferSet> m_TempA;
	std::shared_ptr<VulkanBufferSet> m_TempB;
};