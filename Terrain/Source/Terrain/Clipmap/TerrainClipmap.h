#pragma once

#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "Graphics/Vulkan/VulkanImage.h"
#include "Terrain/Terrain.h"

#include "ClipmapData.h"
#include "DynamicClipmapDeserializer.h"

class TerrainClipmap
{
public:
	TerrainClipmap(const ClipmapTerrainSpecification& spec, const std::unique_ptr<TerrainData>& terrainData);
	~TerrainClipmap() {}

	void Refresh(const glm::vec2& cameraPosition);

	// Loads all the nodes using a separate commandbuffer
	void hardLoad(const glm::vec2& cameraPosition);

	void prepareForDeserialization(VkCommandBuffer cmdBuffer);
	void prepareForRendering(VkCommandBuffer cmdBuffer);

	const ClipmapTerrainSpecification& getSpecification() { return m_Specification; }
	const std::shared_ptr<VulkanImage>& getMap() { return m_Map; }
	const glm::ivec2& getLastValidCameraPosition() { return m_LastValidCameraPosition; }

	void blitNodes(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer, const std::vector<VkBufferImageCopy>& regions);
	
	// returns true if the last valid position was updated, false otherwise
	bool updateClipmaps(VkCommandBuffer cmdBuffer);

private:
	void getLODBounds(int32_t lod, const glm::ivec2& camPositionSnapped, glm::ivec2& xBounds, glm::ivec2& yBounds);

private:
	ClipmapTerrainSpecification m_Specification;
	const std::unique_ptr<TerrainData>& m_TerrainData;

	std::shared_ptr<VulkanImage> m_Map;
	std::shared_ptr<DynamicClipmapDeserializer> m_Deserializer;

	std::unordered_set<size_t> m_LoadedNodes;
	
	glm::ivec2 m_LastCameraPosition = { -1000, -1000 };
	glm::ivec2 m_LastValidCameraPosition;

	int32_t m_RingSize;
};