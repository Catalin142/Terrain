#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "Terrain/Generator/TerrainGenerator.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Terrain/TerrainChunk.h"
#include "VMData.h"

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

class DynamicVirtualTerrainDeserializer;

// offline - reads data from disk
// we should have only one instance that includes all terrain virtual textures.
// only one indirection texture for all virtual textures
class TerrainVirtualMap
{
public:
	TerrainVirtualMap(const VirtualTerrainMapSpecification& spec);

	const VirtualTerrainMapSpecification& getSpecification() {
		return m_Specification;
	}

	void pushLoadTasks(const std::vector<TerrainChunk>& chunks);
	// this is best to keep in a separate command buffer, and start the main command buffer just after this finishes
	void updateVirtualMap(VkCommandBuffer cmdBuffer);

	void blitNodes(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer, const std::vector<VkBufferImageCopy>& regions);

	const std::shared_ptr<VulkanImage>& getPhysicalTexture() { return m_PhysicalTexture; }
	const std::shared_ptr<VulkanImage>& getIndirectionTexture() { return m_IndirectionTexture; }
	const std::shared_ptr<VulkanImage>& getLoadStatusTexture() { return m_StatusTexture; }

	void addVirtualChunkProperty(size_t chunk, const VirtualTerrainChunkProperties& props);

	void updateResources(VkCommandBuffer cmdBuffer);

	void prepareForDeserialization(VkCommandBuffer cmdBuffer);
	void prepareForRendering(VkCommandBuffer cmdBuffer);

	void getChunksToLoad(const glm::vec2& camPosition, std::vector<TerrainChunk>& chunks);

private:
	void createIndirectionResources();
	void createStatusResources();

	void updateIndirectionTexture(VkCommandBuffer cmdBuffer);
	void updateStatusTexture(VkCommandBuffer cmdBuffer);

private:
	std::shared_ptr<VulkanImage> m_PhysicalTexture;

	VirtualTerrainMapSpecification m_Specification;
	std::unordered_map<size_t, VirtualTerrainChunkProperties> m_ChunkProperties;

	// I keep a cache of the last Slot a Chunk occupied and the last chunk in a specified slot
	std::unordered_map<size_t, int32_t> m_LastChunkSlot;
	std::vector<size_t> m_LastSlotChunk;

	std::unordered_set<size_t> m_ActiveNodes;
	std::unordered_set<int32_t> m_AvailableSlots;

	std::unordered_set<size_t> m_NodesToUnload;

	std::shared_ptr<VulkanImage> m_IndirectionTexture;
	std::shared_ptr<VulkanComputePass> m_UpdateIndirectionComputePass;
	std::shared_ptr<VulkanBuffer> m_IndirectionNodesStorage;
	std::vector<GPUIndirectionNode> m_IndirectionNodes;

	std::shared_ptr<VulkanImage> m_StatusTexture;
	std::shared_ptr<VulkanComputePass> m_UpdateStatusComputePass;
	std::shared_ptr<VulkanBuffer> m_StatusNodesStorage;
	std::vector<GPUStatusNode> m_StatusNodes;

	std::shared_ptr<DynamicVirtualTerrainDeserializer> m_Deserializer;

	// TODO(if needed): hold a metadata storage buffer and hold the node index in a texture
};