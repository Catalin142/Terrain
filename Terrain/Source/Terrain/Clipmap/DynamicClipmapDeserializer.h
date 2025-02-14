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

	uint32_t loadedChunks() { return m_MemoryIndex; }
	const std::shared_ptr<VulkanBuffer>& getImageData() { return m_RawImageData[0]; }
	const std::vector<VkBufferImageCopy>& getRegions() { return m_RegionsToCopy; }

	bool Refresh(VkCommandBuffer cmdBuffer, TerrainClipmap* clipmap);

	const glm::ivec2& getLastValidPosition() { return m_LastValidPosition; }

private:
	VkBufferImageCopy createRegion(const FileChunkProperties& task);

private:
	ClipmapTerrainSpecification m_Specification;
	std::string m_ChunksDataFilepath;
	int32_t m_ChunkSize; 

	std::thread m_LoadThread;

	std::condition_variable m_PositionsCV;

	std::mutex m_DataMutex;
	std::mutex m_CopyDataMutex;

	bool m_ThreadRunning = true;

	uint32_t m_MemoryIndex = 0;
	uint32_t m_AvailableBuffer = 0;
	uint32_t m_TextureDataStride;
	
	std::queue<ClipmapLoadTask> m_LoadTasks;
	std::queue<glm::ivec2> m_PositionsToProcess;
	
	std::vector<VkBufferImageCopy> m_RegionsToCopy;
	std::vector<std::shared_ptr<VulkanBuffer>> m_RawImageData;

	glm::ivec2 m_LastValidPosition;

	std::ifstream* m_FileHandle = nullptr;
};