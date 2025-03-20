#pragma once

#include "Graphics/Vulkan/VulkanImage.h"

#include <string>
#include <memory>

struct SerializeChunkedSettings
{
	std::string MetadataFilepath;
	std::string RawdataFilepath;

	uint32_t ChunkSize = 128;
	uint32_t LODs = 0;
};

class HeightMapDiskProcessor
{
public:
	HeightMapDiskProcessor(const std::string& filepath, uint32_t lods);

	void serializeChunked(const SerializeChunkedSettings& settings);

private:
	std::shared_ptr<VulkanImage> m_HeightMap = nullptr;

	int32_t m_Width;
	int32_t m_Height;
	int32_t m_Channels;
};