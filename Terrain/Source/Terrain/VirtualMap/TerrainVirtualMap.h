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
#define MAX_LOD 6u

struct VirtualTextureLocation
{
	std::string Data;
	std::string Table;
};

struct VirtualTerrainMapSpecification
{
	uint32_t VirtualTextureSize = 1024;
	uint32_t ChunkSize = 128;

	// Must be <more than >= 128
	uint32_t IndirectionTextureSize = 64;
	
	uint32_t PhysicalTextureSize = 2048;
	uint32_t LODCount = 4; 
	VkFormat Format;

	VirtualTextureLocation Filepath;
};

struct OnFileChunkProperties
{
	size_t Offset;
	uint32_t Size;
};

struct ChunkHashValues
{
	uint32_t WorldOffset;
	uint32_t Mip;
};

struct NodeData
{
	size_t Node;
	std::vector<char> Data;
};

class TerrainVirtualMap;

struct ChunkLoadTask
{
	TerrainVirtualMap* VirtualMap;
	size_t Node;
	VirtualTextureLocation Location;
	OnFileChunkProperties OnFileProperties;
};

struct NodeToBlit
{
	NodeData Node;
	TerrainVirtualMap* Destination;
};

class VirtualTerrainSerializer;

// offline - reads data from disk
// we should have only one instance that includes all terrain virtual textures.
// only one indirection texture for all virtual textures
class TerrainVirtualMap
{
public:
	TerrainVirtualMap(const VirtualTerrainMapSpecification& spec);
	~TerrainVirtualMap() {}

	const VirtualTerrainMapSpecification& getSpecification() {
		return m_Specification;
	}

	void updateVirtualMap(const std::vector<TerrainChunk>& chunks);
	void blitNode(NodeData& nodeData, VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer);

	void addChunkProperty(size_t chunk, const OnFileChunkProperties& prop);

	std::shared_ptr<VulkanImage> m_PhysicalTexture;
	std::shared_ptr<VulkanImage> m_IndirectionTexture;

	void updateIndirectionTexture(VkCommandBuffer cmdBuffer, std::vector<LoadedNode>& nodes);

private:
	void refreshNodes();

	void createCompute();

private:
	VirtualTerrainMapSpecification m_Specification;

	// Hash(Offset, Mip)
	std::unordered_map<size_t, OnFileChunkProperties> m_ChunkProperties;

	// I Kepp a cache of the last Slot a Chunk occupied and the last chunk in a specified slot
	std::unordered_map<size_t, int32_t> m_LastChunkSlot;
	std::vector<size_t> m_LastSlotChunk;

	std::unordered_set<size_t> m_ActiveNodes;
	std::unordered_set<int32_t> m_AvailableSlots;

	std::unordered_set<size_t> m_NodesToUnload;

	std::shared_ptr<VulkanComputePass> m_IndirectionTextureUpdatePass;
	std::shared_ptr<VulkanUniformBuffer> m_LoadedNodesUB;
	VirtualMapProperties m_VMProps{ };
};


