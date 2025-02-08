#pragma once

#include <thread>
#include <vector>
#include <semaphore>
#include <queue>
#include <mutex>
#include <fstream>
#include <cstdint>

#include <vulkan/vulkan.h>

#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Terrain/TerrainChunk.h"

#include "ClipmapData.h"

#define MAX_CHUNKS_LOADING_PER_FRAME 64

// choosing to use more threads for this is bcs before rendering a frame, we need all the chunks loaded
#define LOAD_THREADS_COUNT 2

class DynamicClipmapDeserializer
{
public:
	DynamicClipmapDeserializer(const ClipmapSpecification& spec);

	void loadChunk(const FileChunkProperties& task);
	void Flush();

	uint32_t loadedChunks() { return m_MemoryIndex; }
	const std::shared_ptr<VulkanBuffer>& getImageData() { return m_RawImageData; }
	const std::vector<VkBufferImageCopy>& getRegions() { return m_RegionsToCopy; }

private:
	ClipmapSpecification m_Specification;

	std::queue<ClipmapLoadTask> m_LoadTasks;

	std::vector<VkBufferImageCopy> m_RegionsToCopy;
	std::shared_ptr<VulkanBuffer> m_RawImageData;
	uint32_t m_MemoryIndex = 0;

	std::ifstream* m_FileHandle = nullptr;

	uint32_t m_TextureDataStride;
};