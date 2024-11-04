#pragma once

#include "TerrainVirtualMap.h"

#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>

// Data gets laoded to the virtual map in batches (for each batch i allocate a vulkan buffer to hold data)
#define LOAD_BATCH_SIZE 8

// we only need one instance of this class that will work on only one instance of virtual map
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
	// Do we need this to be singleton? Maybe create a vm terrain manager to manage the loading and unloading and everything on a update cycle
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

	std::shared_ptr<VulkanComputePass> m_IndirectionTextureUpdatePass;
	std::shared_ptr<VulkanUniformBuffer> m_LoadedNodesUB;
	VirtualMapProperties m_VMProps{ };
};