#pragma once

#include "TerrainVirtualMap.h"

#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>

// Data gets laoded to the virtual map in batches (for each batch i allocate a vulkan buffer to hold data)
#define LOAD_BATCH_SIZE 8

// TODO: fix the case when we load more than 128
#define MAX_CHUNKS_LOADING_PER_FRAME 128

struct NodeData
{
	size_t Node;
	uint32_t VirtualLocation;
	uint32_t Mip;
	VirtualTextureType Type;
	uint32_t MemoryIndex = 0;
};

// we only need one instance of this class that will work on only one instance of virtual map
class DynamicVirtualTerrainDeserializer
{
public:
	static DynamicVirtualTerrainDeserializer* Get() {
		if (m_Instance == nullptr)
			m_Instance = new DynamicVirtualTerrainDeserializer();
		return m_Instance;
	}

	void Initialize(const std::shared_ptr<TerrainVirtualMap>& vm);
	void Shutdown();

	// The deserializer loads data from the file on a different thread and holds it until i refresh the deserializer
	// on refresh, every loaded node gets blited to it s destination virtual map 
	void Refresh();

	void loadChunk(NodeData task);
	void pushLoadTask(size_t node, uint32_t virtualLocation, uint32_t mip, VirtualTextureType type);

private:
	// Do we need this to be singleton? Maybe create a vm terrain manager to manage the loading and unloading and everything on a update cycle
	static DynamicVirtualTerrainDeserializer* m_Instance;

	std::shared_ptr<TerrainVirtualMap> m_VirtualMap;

	std::thread m_LoadThread;
	std::mutex m_DataMutex;
	std::mutex m_TaskMutex;
	std::mutex m_SlotsMutex;

	bool m_ThreadRunning = true;

	std::vector<NodeData> m_NodesToBlit;
	std::queue<NodeData> m_LoadTasks;

	std::vector<std::shared_ptr<VulkanBuffer>> m_BufferPool;

	// Cache of file handlers
	// IDK if it impacts performance if i keep them open all the time
	// Guess it s better than opening and closing them all the time
	std::unordered_map<std::string, std::ifstream*> m_FileHandlerCache;

	std::shared_ptr<VulkanComputePass> m_IndirectionTextureUpdatePass;
	std::shared_ptr<VulkanUniformBuffer> m_LoadedNodesUB;
	VirtualMapProperties m_VMProps{ };

	// have a big buffer of data to load and unload nodes in
	char* m_TextureDataBuffer;
	// in memory buffer
	std::queue<uint32_t> m_AvailableSlots;
	uint32_t m_TextureDataStride = 0;
	uint32_t m_OccupiedSlots = 0;
};