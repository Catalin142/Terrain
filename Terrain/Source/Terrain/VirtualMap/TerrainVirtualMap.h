#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "Terrain/Generator/TerrainGenerator.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Terrain/TerrainChunk.h"
#include "VirtualMapData.h"
#include "Terrain/Terrain.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <set>
#include <functional>
#include <mutex>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <string>

#define INVALID_SLOT -1
#define INVALID_SLOT -1

class DynamicVirtualTerrainDeserializer;

// offline - reads data from disk
// we should have only one instance that includes all terrain virtual textures.
// only one indirection texture for all virtual textures
class TerrainVirtualMap
{
public:
	TerrainVirtualMap(const VirtualTerrainMapSpecification& spec, const std::unique_ptr<TerrainData>& terrainData);

	const VirtualTerrainMapSpecification& getSpecification() {
		return m_Specification;
	}

	uint32_t pushLoadTasks(const glm::vec2& camPosition);
	void createLoadTask(const TerrainChunk& chunks);

	uint32_t updateMap(VkCommandBuffer cmdBuffer);

	void blitNodes(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer, const std::vector<VkBufferImageCopy>& regions);

	const std::shared_ptr<VulkanImage>& getPhysicalTexture() { return m_PhysicalTexture; }
	const std::shared_ptr<VulkanImage>& getIndirectionTexture() { return m_IndirectionTexture; }
	const std::shared_ptr<VulkanImage>& getLoadStatusTexture() { return m_StatusTexture; }

	void updateIndirectionTexture(VkCommandBuffer cmdBuffer);
	void updateStatusTexture(VkCommandBuffer cmdBuffer);

	void prepareForDeserialization(VkCommandBuffer cmdBuffer);
	void prepareForRendering(VkCommandBuffer cmdBuffer);

	bool Updated();

private:
	void createIndirectionResources();
	void createStatusResources();

private:
	std::shared_ptr<VulkanImage> m_PhysicalTexture;
	const std::unique_ptr<TerrainData>& m_TerrainData;
	TerrainInfo m_TerrainInfo;

	std::queue<glm::ivec2> m_PositionsToProcess;

	VirtualTerrainMapSpecification m_Specification;

	// I keep a cache of the last Slot a Chunk occupied and the last chunk in a specified slot
	std::unordered_map<size_t, int32_t> m_LastChunkSlot;
	std::vector<size_t> m_LastSlotChunk;

	std::unordered_set<size_t> m_ActiveNodesCPU;
	std::unordered_set<int32_t> m_AvailableSlots;

	std::unordered_set<size_t> m_NodesToUnload;

	std::shared_ptr<VulkanImage> m_IndirectionTexture;
	VulkanComputePass m_UpdateIndirectionComputePass;
	std::shared_ptr<VulkanBufferSet> m_IndirectionNodesStorage;
	std::vector<GPUIndirectionNode> m_IndirectionNodes;

	std::shared_ptr<VulkanImage> m_StatusTexture;
	VulkanComputePass m_UpdateStatusComputePass;
	std::shared_ptr<VulkanBufferSet> m_StatusNodesStorage;
	std::vector<GPUStatusNode> m_StatusNodes;

	std::shared_ptr<DynamicVirtualTerrainDeserializer> m_Deserializer;

	glm::ivec2 m_LastCameraPosition = { -1, -1 };
	bool m_Updated = false;
};