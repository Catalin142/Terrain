#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/VirtualMap/TerrainVirtualMap.h"

#include "Graphics/Vulkan/VulkanComputePipeline.h"
#include "Graphics/Vulkan/VulkanBuffer.h"

#include "Scene/Camera.h"

#include <vector>
#include <memory>

// Technique highly inspired by Far Cry 5: https://www.gdcvault.com/play/1025261/Terrain-Rendering-in-Far-Cry
struct QuadTreePassMetadata
{
	uint32_t TMPArray1Index;
	uint32_t ResultArrayIndex;
	uint32_t DataLoaded;
};

class QuadTreeLOD
{
public:
	QuadTreeLOD(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<TerrainVirtualMap>& virtualMap);
	~QuadTreeLOD() = default;

	void Generate(VkCommandBuffer commandBuffer, std::vector<TerrainChunk> firstPass, uint32_t bufferIndex);
	void FrustumCull(VkCommandBuffer commandBuffer, Camera cam, uint32_t bufferIndex);

private:
	void createQuadTreePass(const std::shared_ptr<TerrainVirtualMap>& virtualMap);
	void createFrustumCullingPass(const std::shared_ptr<VulkanBuffer>& infoBuffer);

public:
	std::shared_ptr<VulkanBuffer> ChunksToRender;
	std::shared_ptr<VulkanBufferSet> PassMetadata;
	std::shared_ptr<VulkanBuffer> m_RenderCommand;
	std::shared_ptr<VulkanBuffer> AllChunks;

private:
	TerrainSpecification m_TerrainSpecification;

	std::shared_ptr<VulkanComputePipeline> m_ConstructQuadTreePipeline;
	std::shared_ptr<VulkanComputePipeline> m_FrustumCull;
	std::vector<std::shared_ptr<VulkanDescriptorSet>> m_DescriptorSets;
	
	std::shared_ptr<VulkanBufferSet> m_TempA;
	std::shared_ptr<VulkanBufferSet> m_TempB;
	std::shared_ptr<VulkanBuffer> m_DispatchCommand;

	VulkanComputePass m_FrustumCullingPass;
	std::shared_ptr<VulkanBuffer> m_FrustumBuffer;
	
};