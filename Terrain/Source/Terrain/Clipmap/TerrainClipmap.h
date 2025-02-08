#pragma once

#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "Graphics/Vulkan/VulkanImage.h"

#include "ClipmapData.h"
#include "DynamicClipmapDeserializer.h"

class TerrainClipmap
{
public:
	TerrainClipmap(const ClipmapSpecification& spec);
	~TerrainClipmap() {}

	void Refresh(VkCommandBuffer commandBuffer, const glm::vec2& cameraPosition);

	// Loads all the nodes using a separate commandbuffer
	void hardLoad(const glm::vec2& cameraPosition);

	void prepareForDeserialization(VkCommandBuffer cmdBuffer);
	void prepareForRendering(VkCommandBuffer cmdBuffer);

	void addChunkProperty(size_t chunk, const FileChunkProperties& props);

	const ClipmapSpecification& getSpecification() { return m_Specification; }

	const std::shared_ptr<VulkanImage>& getMap() { return m_Map; }

private:
	ClipmapSpecification m_Specification;

	std::shared_ptr<VulkanImage> m_Map;
	std::shared_ptr<DynamicClipmapDeserializer> m_Deserializer;

	std::unordered_set<uint32_t> m_LoadedNodes;

	std::unordered_map<size_t, FileChunkProperties> m_ChunkProperties;
	
	// put on a random value, small chances it s equal to the first cam pos
	glm::ivec2 m_LastCameraPosition = { -142, -12 };
};