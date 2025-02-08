#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "TerrainVirtualMap.h"
#include "Terrain/Terrain.h"
#include "Terrain/Clipmap/TerrainClipmap.h"

#include <memory>

// TODO: implement this
struct SerializeSettings
{
	std::shared_ptr<VulkanImage> heightMap = nullptr;
	std::shared_ptr<VulkanImage> normalMap = nullptr;
	std::shared_ptr<VulkanImage> compositionMap = nullptr;
	uint32_t textureSize = 1024;

	std::string metadataFilepath;
	std::string rawdataFilepath;

	uint32_t chunkSize = 128;
	glm::uvec2 worldOffset = { 0u, 0u };
	uint32_t mips = 0;
};

class VirtualTerrainSerializer
{
	friend class TerrainVirtualMap;

public:
	static void Serialize(const SerializeSettings& settings);

	static void Serialize(const std::shared_ptr<VulkanImage>& texture, const std::string& table, const std::string& data, uint32_t chunkSize,
		glm::uvec2 worldOffset = { 0u, 0u }, bool purgeContent = true);

	static void SerializeClipmap(const std::shared_ptr<VulkanImage>& texture, const std::string& table, const std::string& data, uint32_t chunkSize,
		glm::uvec2 worldOffset = { 0u, 0u }, bool purgeContent = true);

	static void Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap);
	static void Deserialize(const std::shared_ptr<TerrainClipmap>& virtualMap);
	static void Deserialize(std::unordered_map<size_t, FileChunkProperties>& virtualMap, std::string tab);
};