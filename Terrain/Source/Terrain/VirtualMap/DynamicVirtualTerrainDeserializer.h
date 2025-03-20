#pragma once

#include "VirtualMapData.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Terrain/TerrainChunk.h"

#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <semaphore>
#include <mutex>
#include <vulkan/vulkan.h>

#define MAX_CHUNKS_LOADING_PER_FRAME 64

class TerrainVirtualMap;

class DynamicVirtualTerrainDeserializer
{
public:
	DynamicVirtualTerrainDeserializer(const VirtualTerrainMapSpecification& spec, int32_t chunkSize, const std::string& filepath);
	~DynamicVirtualTerrainDeserializer();

	// The deserializer loads data from the file on a different thread and holds it until i refresh the deserializer
	// on refresh, every loaded node gets blited to it s destination virtual map 
	void Refresh(VkCommandBuffer cmdBuffer, TerrainVirtualMap* virtualMap);

	void loadChunk(VirtualMapLoadTask task);
	void pushLoadTask(size_t node, int32_t virtualLocation, const FileChunkProperties& properties);
	
	bool isAvailable() { return m_Available; }

private:
	VkBufferImageCopy createRegion(const VirtualMapLoadTask& task);

public:
	VirtualMapDeserializerLastUpdate LastUpdate;

private:
	VirtualTerrainMapSpecification m_VirtualMapSpecification;
	std::string m_ChunksDataFilepath;
	int32_t m_ChunkSize;

	std::thread m_LoadThread;
	std::mutex m_DataMutex;
	std::mutex m_TaskMutex;

	std::counting_semaphore<16000> m_LoadThreadSemaphore{ 0 };
	std::condition_variable m_PositionsCV;
	bool m_ThreadRunning = true;

	bool m_Available = true;
	
	std::vector<VkBufferImageCopy> m_RegionsToCopy;
	std::queue<VirtualMapLoadTask> m_LoadTasks;

	std::vector<std::shared_ptr<VulkanBuffer>> m_RawImageData;
	uint32_t m_AvailableBuffer = 0;
	uint32_t m_MemoryIndex = 0;

	// Cache of file handlers
	// IDK if it impacts performance if i keep them open all the time
	// Guess it s better than opening and closing them all the time
	std::unordered_map<std::string, std::ifstream*> m_FileHandlerCache;

	// in memory buffer
	uint32_t m_TextureDataStride = 0;

	// Compute shader to update the state of the loaded/not loaded texture

	// I prefer having different vectors for these 2 so I can create them on the fly and just send them to the GPU
	std::vector<GPUIndirectionNode> m_IndirectionNodes;
	std::vector<GPUStatusNode> m_StatusNodes;
};