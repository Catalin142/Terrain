#include "TerrainClipmap.h"

#include <fstream>

#include "Graphics/Vulkan/VulkanUtils.h"
#include "Core/Hash.h"

TerrainClipmap::TerrainClipmap(const ClipmapSpecification& spec) : m_Specification(spec)
{
	{
		VulkanImageSpecification clipmapSpecification{};
		clipmapSpecification.Width = m_Specification.TextureSize;
		clipmapSpecification.Height = m_Specification.TextureSize;
		clipmapSpecification.Format = VK_FORMAT_R16_SFLOAT;
		clipmapSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		clipmapSpecification.LayerCount = m_Specification.LODCount;
		clipmapSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;;
		clipmapSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		m_Map = std::make_shared<VulkanImage>(clipmapSpecification);
		m_Map->Create();
	}

	m_Deserializer = std::make_shared<DynamicClipmapDeserializer>(m_Specification);
}

#include <iostream>

void TerrainClipmap::Refresh(VkCommandBuffer commandBuffer, const glm::vec2& cameraPosition)
{
	glm::ivec2 intCameraPos = cameraPosition;

	if (m_LastCameraPosition / (int32_t)m_Specification.ChunkSize == intCameraPos / (int32_t)m_Specification.ChunkSize)
		return;

	std::unordered_set<uint32_t> m_NodesToUnload = m_LoadedNodes;
	m_LastCameraPosition = cameraPosition;

	prepareForDeserialization(commandBuffer);

	int32_t ringSize = m_Specification.TextureSize / m_Specification.ChunkSize;
	float fRingSize = ringSize;

	int32_t terrainSize = m_Specification.TerrainSize;
	float fTerrainSize = (float)terrainSize;
	
	glm::vec2 terrainCameraPosition = cameraPosition;
	for (int32_t lod = 0; lod < m_Specification.LODCount; lod++)
	{
		TerrainChunk tc;
		tc.Lod = lod;

		int32_t chunkSize = m_Specification.ChunkSize * (1 << lod);

		glm::ivec2 snapedPosition;

		float fChunkSize = (float)chunkSize;
		terrainCameraPosition.x = glm::max(terrainCameraPosition.x, fChunkSize * (fRingSize / 2.0f));
		terrainCameraPosition.y = glm::max(terrainCameraPosition.y, fChunkSize * (fRingSize / 2.0f));
		terrainCameraPosition.x = glm::min(terrainCameraPosition.x, fTerrainSize - fChunkSize * (fRingSize / 2.0f));
		terrainCameraPosition.y = glm::min(terrainCameraPosition.y, fTerrainSize - fChunkSize * (fRingSize / 2.0f));

		snapedPosition.x = int32_t(terrainCameraPosition.x) / chunkSize;
		snapedPosition.y = int32_t(terrainCameraPosition.y) / chunkSize;

		int32_t minY = glm::max(snapedPosition.y - ringSize / 2, 0);
		int32_t maxY = glm::min(snapedPosition.y + ringSize / 2, terrainSize / chunkSize);

		int32_t minX = glm::max(snapedPosition.x - ringSize / 2, 0);
		int32_t maxX = glm::min(snapedPosition.x + ringSize / 2, terrainSize / chunkSize);

		if (snapedPosition.x % 2 != 0)
		{
			minX -= 1;
			maxX -= 1;
		}

		if (snapedPosition.y % 2 != 0)
		{
			minY -= 1;
			maxY -= 1;
		}

		for (int32_t y = minY; y < maxY; y++)
			for (int32_t x = minX; x < maxX; x++)
			{
				size_t chunkId = getChunkID(x, y, lod);

				if (m_NodesToUnload.find(chunkId) != m_NodesToUnload.end())
					m_NodesToUnload.erase(chunkId);

				if (m_LoadedNodes.find(chunkId) != m_LoadedNodes.end())
					continue;

				m_LoadedNodes.insert(chunkId);

				m_Deserializer->loadChunk(m_ChunkProperties[chunkId]);
				assert(m_Deserializer->loadedChunks() != MAX_CHUNKS_LOADING_PER_FRAME);
			}
	}

	if (m_Deserializer->loadedChunks() != 0)
	{
		m_Map->batchCopyBuffer(commandBuffer, *m_Deserializer->getImageData(), m_Deserializer->getRegions());
		m_Deserializer->Flush();
	}

	prepareForRendering(commandBuffer);

	for (size_t chunk : m_NodesToUnload)
		m_LoadedNodes.erase(chunk);
}

void TerrainClipmap::hardLoad(const glm::vec2& cameraPosition)
{
	m_LastCameraPosition = cameraPosition;

	VkCommandBuffer currentCommandBuffer = VkUtils::beginSingleTimeCommand();
	prepareForDeserialization(currentCommandBuffer);

	int32_t ringSize = m_Specification.TextureSize / m_Specification.ChunkSize;
	float fRingSize = ringSize;

	int32_t terrainSize = m_Specification.TerrainSize;
	float fTerrainSize = (float)terrainSize;

	glm::vec2 terrainCameraPosition = cameraPosition;
	for (int32_t lod = 0; lod < m_Specification.LODCount; lod++)
	{
		TerrainChunk tc;
		tc.Lod = lod;

		int32_t chunkSize = m_Specification.ChunkSize * (1 << lod);

		glm::ivec2 snapedPosition;

		float fChunkSize = (float)chunkSize;
		terrainCameraPosition.x = glm::max(terrainCameraPosition.x, fChunkSize * (fRingSize / 2.0f));
		terrainCameraPosition.y = glm::max(terrainCameraPosition.y, fChunkSize * (fRingSize / 2.0f));
		terrainCameraPosition.x = glm::min(terrainCameraPosition.x, fTerrainSize - fChunkSize * (fRingSize / 2.0f));
		terrainCameraPosition.y = glm::min(terrainCameraPosition.y, fTerrainSize - fChunkSize * (fRingSize / 2.0f));

		snapedPosition.x = int32_t(terrainCameraPosition.x) / chunkSize;
		snapedPosition.y = int32_t(terrainCameraPosition.y) / chunkSize;

		int32_t minY = glm::max(snapedPosition.y - ringSize / 2, 0);
		int32_t maxY = glm::min(snapedPosition.y + ringSize / 2, terrainSize / chunkSize);

		int32_t minX = glm::max(snapedPosition.x - ringSize / 2, 0);
		int32_t maxX = glm::min(snapedPosition.x + ringSize / 2, terrainSize / chunkSize);

		if (snapedPosition.x % 2 != 0)
		{
			minX -= 1;
			maxX -= 1;
		}

		if (snapedPosition.y % 2 != 0)
		{
			minY -= 1;
			maxY -= 1;
		}

		for (int32_t y = minY; y < maxY; y++)
			for (int32_t x = minX; x < maxX; x++)
			{
				size_t chunkId = getChunkID(x, y, lod);

				m_LoadedNodes.insert(chunkId);

				m_Deserializer->loadChunk(m_ChunkProperties[chunkId]);
				if (m_Deserializer->loadedChunks() == MAX_CHUNKS_LOADING_PER_FRAME)
				{
					m_Map->batchCopyBuffer(currentCommandBuffer, *m_Deserializer->getImageData(), m_Deserializer->getRegions());
					m_Deserializer->Flush();

					VkUtils::endSingleTimeCommand(currentCommandBuffer);
					currentCommandBuffer = VkUtils::beginSingleTimeCommand();
				}
			}
	}

	if (m_Deserializer->loadedChunks() != 0)
	{
		m_Map->batchCopyBuffer(currentCommandBuffer, *m_Deserializer->getImageData(), m_Deserializer->getRegions());
		m_Deserializer->Flush();
	}

	prepareForRendering(currentCommandBuffer);

	VkUtils::endSingleTimeCommand(currentCommandBuffer);
}

void TerrainClipmap::prepareForDeserialization(VkCommandBuffer cmdBuffer)
{
	VkImageSubresourceRange imgSubresource{};
	imgSubresource.aspectMask = m_Map->getSpecification().Aspect;
	imgSubresource.layerCount = m_Specification.LODCount;
	imgSubresource.levelCount = 1;
	imgSubresource.baseMipLevel = 0;
	VkUtils::transitionImageLayout(cmdBuffer, m_Map->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void TerrainClipmap::prepareForRendering(VkCommandBuffer cmdBuffer)
{
	VkImageSubresourceRange imgSubresource{};
	imgSubresource.aspectMask = m_Map->getSpecification().Aspect;
	imgSubresource.layerCount = m_Specification.LODCount;
	imgSubresource.levelCount = 1;
	imgSubresource.baseMipLevel = 0;
	VkUtils::transitionImageLayout(cmdBuffer, m_Map->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
}

void TerrainClipmap::addChunkProperty(size_t chunk, const FileChunkProperties& props)
{
	m_ChunkProperties[chunk] = props;
}
