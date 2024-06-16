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
#include <mutex>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <string>

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

class VirtualTerrainSerializer;

#define INVALID_SLOT -1

// offline - reads data from disk
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

private:
	void refreshNodes();

private:
	VirtualTerrainMapSpecification m_Specification;

	// Hash(Offset, Mip)
	std::unordered_map<size_t, OnFileChunkProperties> m_ChunkProperties;

	// I Kepp a cache of the last Slot a Chunk occupied and the last chunk in a specified slot
	std::unordered_map<size_t, int32_t> m_LastChunkSlot;
	std::vector<size_t> m_LastSlotChunk;

	std::unordered_set<size_t> m_ActiveNodes;
	std::unordered_set<int32_t> m_AvailableSlots;

	// I created them here instead each frame in the function, ig it saves performance, maybe?
	std::unordered_set<size_t> m_NodesToUnload;
};

// Data gets laoded to the virtual map in batches (for each batch i allocate a vulkan buffer to hold data)
#define LOAD_BATCH_SIZE 8

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

class VirtualTerrainSerializer
{
	friend class TerrainVirtualMap;

public:
	static void Serialize(const std::shared_ptr<VulkanImage>& map, const VirtualTerrainMapSpecification& spec, glm::uvec2 worldOffset = { 0u, 0u }, bool purgeContent = true);
	static void Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap);
};

class DynamicVirtualTerrainDeserializer
{
public:
	static DynamicVirtualTerrainDeserializer* Get() {
		if (m_Instance == nullptr)
			m_Instance = new DynamicVirtualTerrainDeserializer();
		return m_Instance;
	}

	void Initialize();
	void Shutdown();

	// The deserializer loads data from the file on a different thread and holds it until i refresh the deserializer
	// on refresh, every loaded node gets blited to it s destination virtual map 
	void Refresh();

	void loadChunk(const ChunkLoadTask& task);
	void pushLoadTask(TerrainVirtualMap* vm, size_t node, const VirtualTextureLocation& location, OnFileChunkProperties onFileProps);

private:
	static DynamicVirtualTerrainDeserializer* m_Instance;

	std::thread m_LoadThread;
	std::mutex m_DataMutex;
	std::mutex m_TaskMutex;

	bool m_ThreadRunning = true;

	std::vector<NodeToBlit> m_NodesToBlit;
	std::queue<ChunkLoadTask> m_LoadTasks;

	std::vector<std::shared_ptr<VulkanBuffer>> m_BufferPool;

	// Cache of file handlers
	// IDK if it impacts performance if i keep them open all the time
	// Guess it s better than opening and closing them all the time
	std::unordered_map<std::string, std::ifstream*> m_FileHandlerCache;
};
