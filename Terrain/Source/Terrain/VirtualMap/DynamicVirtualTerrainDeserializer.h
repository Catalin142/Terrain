#pragma once

#include "VirtualMapUtils.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Terrain/TerrainChunk.h"

#include <thread>
#include <vector>
#include <queue>
#include <unordered_map>
#include <semaphore>
#include <mutex>
#include <vulkan/vulkan.h>

#define MAX_CHUNKS_LOADING_PER_FRAME 64

class TerrainVirtualMap;

class DynamicVirtualTerrainDeserializer
{
public:
	DynamicVirtualTerrainDeserializer(const VirtualTerrainMapSpecification& spec);
	~DynamicVirtualTerrainDeserializer();

	// The deserializer loads data from the file on a different thread and holds it until i refresh the deserializer
	// on refresh, every loaded node gets blited to it s destination virtual map 
	void Refresh(VkCommandBuffer cmdBuffer, TerrainVirtualMap* virtualMap);

	void loadChunk(LoadTask task);
	void pushLoadTask(size_t node, int32_t virtualLocation, const VirtualTerrainChunkProperties& properties);

public:
	VirtualMapDeserializerLastUpdate LastUpdate;

private:
	VirtualTerrainMapSpecification m_VirtualMapSpecification;

	std::thread m_LoadThread;
	std::mutex m_DataMutex;
	std::mutex m_TaskMutex;
	std::mutex m_SlotsMutex;

	// in case we want to load mare than MAX_CHUNKS_LOADING_PER_FRAME, we stop the loading thread and wait for a batch to be sent to the GPU
	std::counting_semaphore<MAX_CHUNKS_LOADING_PER_FRAME> m_MaxLoadSemaphore{ 0 };
	std::counting_semaphore<1024> m_LoadThreadSemaphore{ 1 };
	bool m_ThreadRunning = true;

	std::vector<VkBufferImageCopy> m_RegionsToCopy;
	std::vector<uint32_t> m_UsedIndices;
	std::queue<LoadTask> m_LoadTasks;

	std::shared_ptr<VulkanBuffer> m_RawImageData;

	// Cache of file handlers
	// IDK if it impacts performance if i keep them open all the time
	// Guess it s better than opening and closing them all the time
	std::unordered_map<std::string, std::ifstream*> m_FileHandlerCache;

	// in memory buffer
	std::queue<uint32_t> m_AvailableSlots;
	uint32_t m_TextureDataStride = 0;

	// Compute shader to update the state of the loaded/not loaded texture

	// I prefer having different vectors for these 2 so I can create them on the fly and just send them to the GPU
	std::vector<GPUIndirectionNode> m_IndirectionNodes;
	std::vector<GPUStatusNode> m_StatusNodes;
};