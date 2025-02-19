#pragma once

#include <thread>
#include <vector>
#include <semaphore>
#include <queue>
#include <deque>
#include <mutex>
#include <fstream>
#include <unordered_map>
#include <cstdint>

#include <vulkan/vulkan.h>

#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Terrain/TerrainChunk.h"

#include "ClipmapData.h"

#define MAX_CHUNKS_LOADING_PER_FRAME 64

class TerrainClipmap;

class DynamicClipmapDeserializer
{
public:
	DynamicClipmapDeserializer(const ClipmapTerrainSpecification& spec, int32_t chunkSize, const std::string& filepath);
	~DynamicClipmapDeserializer();

	void pushLoadChunk(const glm::ivec2& camPos, const FileChunkProperties& task);
	void loadChunk(const FileChunkProperties& task);

	void loadChunkSequential(const FileChunkProperties& task);
	void Flush();

	uint32_t loadedChunksInBuffer() { return m_MemoryIndex; }
	const std::shared_ptr<VulkanBuffer>& getImageData() { return m_RawImageData[0]; }
	const std::vector<VkBufferImageCopy>& getRegions() { return m_RegionsToCopy; }

	uint32_t Refresh(VkCommandBuffer cmdBuffer, TerrainClipmap* clipmap);

	bool isAvailable() { return m_Available; }
	const glm::ivec2& getLastPosition() { return m_LastPositionProcessed; }

private:
	VkBufferImageCopy createRegion(const FileChunkProperties& task);

private:
	ClipmapTerrainSpecification m_Specification;
	std::string m_ChunksDataFilepath;
	int32_t m_ChunkSize; 

	std::counting_semaphore<1024> m_LoadThreadSemaphore{ 0 };
	std::thread m_LoadThread;

	std::mutex m_DataMutex;
	std::mutex m_CopyDataMutex;

	bool m_ThreadRunning = true;
	bool m_Available = true;

	uint32_t m_MemoryIndex = 0;
	uint32_t m_AvailableBuffer = 0;
	uint32_t m_TextureDataStride;
	
	std::queue<ClipmapLoadTask> m_LoadTasks;
	
	std::vector<VkBufferImageCopy> m_RegionsToCopy;
	std::vector<std::shared_ptr<VulkanBuffer>> m_RawImageData;

	glm::ivec2 m_LastPositionProcessed;
	glm::ivec2 m_CurrentPosition;

	std::ifstream* m_FileHandle = nullptr;
};