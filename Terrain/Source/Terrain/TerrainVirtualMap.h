#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "Generator/TerrainGenerator.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "TerrainChunk.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <set>
#include <functional>
#include <thread>

struct VirtualTextureLocation
{
	std::string Data;
	std::string Table;
};

struct VirtualTerrainMapSpecification
{
	uint32_t VirtualTextureSize = 1024;
	uint32_t ChunkSize = 128;
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

class TerrainVirtualSerializer;

#define INVALID_SLOT -1

// offline - reads data from disk
class TerrainVirtualMap
{
	friend class TerrainVirtualSerializer;

public:
	TerrainVirtualMap(const VirtualTerrainMapSpecification& spec);
	~TerrainVirtualMap();

	void updateVirtualMap(const std::vector<TerrainChunk>& chunks);

	VirtualTerrainMapSpecification m_Specification;

	std::shared_ptr<VulkanImage> m_PhysicalTexture;
	std::shared_ptr<VulkanImage> m_IndirectionTexture;
	std::shared_ptr<VulkanImage> m_AuxiliaryTexture;


private:
	void refreshNodes();
	void loadNode(size_t node);
	void pushNodeToLoad(size_t node);

	void blitNode(NodeData& nodeData, VkCommandBuffer cmdBuffer);

private:
	// Hash(Offset, Mip)
	std::unordered_map<size_t, OnFileChunkProperties> m_ChunkProperties;

	// I Kepp a cache of the last Slot a Chunk occupied and the last chunk in a specified slot
	std::unordered_map<size_t, int32_t> m_LastChunkSlot;
	std::vector<size_t> m_LastSlotChunk;
	std::unordered_set<size_t> m_ActiveNodes;
	std::unordered_set<int32_t> m_AvailableSlots;

	// I created them here instead each frame in the function, ig it saves performance, maybe?
	std::unordered_set<size_t> m_NodesToUnload;
	std::vector<size_t> m_NodesToLoad;

	// Thread made for loading nodes
	//std::thread m_LoadThread;

	// Keep track of all the data that needs to be blited in the mega texture
	std::vector<NodeData> m_NodeData;
	bool m_ThreadRunning = true;
};

struct ChunkLoadTask
{
	TerrainVirtualMap* VirtualMap;
	size_t Node;
	VirtualTextureLocation Location;
	OnFileChunkProperties OnFileProperties;
};

class TerrainVirtualSerializer
{
public:
	static void Initialize();

	static void Serialize(const std::shared_ptr<VulkanImage>& map, const VirtualTerrainMapSpecification& spec, glm::uvec2 worldOffset = { 0u, 0u }, bool purgeContent = true);
	static void Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap);

	static void loadChunk(std::vector<char>& dst, const VirtualTextureLocation& location, OnFileChunkProperties onFileProps);
	static void loadChunk(TerrainVirtualMap* vm, size_t node, const VirtualTextureLocation& location, OnFileChunkProperties onFileProps);

	static void loadChunk(const ChunkLoadTask& task, VkCommandBuffer cmdBuffer);

private:
	static std::thread m_LoadThread;
	static bool m_ThreadRunning;

	static std::vector<ChunkLoadTask> m_LoadTasks;
};
