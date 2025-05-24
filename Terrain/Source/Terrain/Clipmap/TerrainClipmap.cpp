#include "TerrainClipmap.h"

#include "Graphics/Vulkan/VulkanUtils.h"
#include "Core/Hash.h"

#include "Terrain/Terrain.h"

#include <fstream>

TerrainClipmap::TerrainClipmap(const ClipmapTerrainSpecification& spec, const std::unique_ptr<TerrainData>& terrainData) :
	m_Specification(spec), m_TerrainData(terrainData)
{
	TerrainInfo terrainInfo = m_TerrainData->getSpecification().Info;
	m_RingSize = m_Specification.ClipmapSize / terrainInfo.ChunkSize;

	{
		VulkanImageSpecification clipmapSpecification{};
		clipmapSpecification.Width = m_Specification.ClipmapSize + m_RingSize * 2;
		clipmapSpecification.Height = m_Specification.ClipmapSize + m_RingSize * 2;
		clipmapSpecification.Format = VK_FORMAT_R16_UNORM;
		clipmapSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		clipmapSpecification.LayerCount = terrainInfo.LODCount;
		clipmapSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		clipmapSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		m_Map = std::make_shared<VulkanImage>(clipmapSpecification);
		m_Map->Create();
	}

	m_Deserializer = std::make_shared<DynamicClipmapDeserializer>(m_Specification, terrainInfo.ChunkSize, m_TerrainData->getSpecification().ChunkedFilepath.Data);

}

void TerrainClipmap::pushLoadTasks(const glm::vec2& cameraPosition)
{
	TerrainInfo terrainInfo = m_TerrainData->getSpecification().Info;

	glm::ivec2 intCameraPos = glm::ivec2(glm::max((int32_t)cameraPosition.x, 0), glm::max((int32_t)cameraPosition.y, 0));
	glm::ivec2 curPosSnapped = intCameraPos / terrainInfo.ChunkSize * terrainInfo.ChunkSize;

	if (m_LastCameraPosition != curPosSnapped)
	{
		m_LastCameraPosition = curPosSnapped;
		m_PositionsToProcess.push(curPosSnapped);
	}

	if (!m_Deserializer->isAvailable() || m_PositionsToProcess.empty())
		return;

	curPosSnapped = m_PositionsToProcess.front();
	m_PositionsToProcess.pop();

	std::unordered_set<size_t> m_NodesToUnload = m_LoadedNodes;
	for (int32_t lod = 0; lod < terrainInfo.LODCount; lod++)
	{
		glm::ivec2 xBounds, yBounds;
		getLODBounds(lod, curPosSnapped, xBounds, yBounds);

		for (int32_t y = yBounds.x; y < yBounds.y; y++)
			for (int32_t x = xBounds.x; x < xBounds.y; x++)
			{
				size_t chunkId = getChunkID(x, y, lod);

				if (m_NodesToUnload.find(chunkId) != m_NodesToUnload.end())
					m_NodesToUnload.erase(chunkId);

				if (m_LoadedNodes.find(chunkId) != m_LoadedNodes.end())
					continue;

				m_Deserializer->pushLoadChunk(curPosSnapped, m_TerrainData->getChunkProperty(chunkId));
				m_LoadedNodes.insert(chunkId);
			}
	}
	for (size_t chunk : m_NodesToUnload)
		m_LoadedNodes.erase(chunk);
}

void TerrainClipmap::hardLoad(const glm::vec2& cameraPosition)
{
	TerrainInfo terrainInfo = m_TerrainData->getSpecification().Info;

	glm::ivec2 intCameraPos = glm::ivec2(glm::max((int32_t)cameraPosition.x, 0), glm::max((int32_t)cameraPosition.y, 0));
	glm::ivec2 curPosSnapped = intCameraPos / terrainInfo.ChunkSize * terrainInfo.ChunkSize;
	
	if (curPosSnapped == m_LastCameraPosition)
		return;

	m_LastCameraPosition = curPosSnapped;
	m_LastValidCameraPosition = curPosSnapped;

	VkCommandBuffer currentCommandBuffer = VulkanUtils::beginSingleTimeCommand();
	prepareForDeserialization(currentCommandBuffer);

	for (int32_t lod = 0; lod < terrainInfo.LODCount; lod++)
	{
		glm::ivec2 xBounds, yBounds;
		getLODBounds(lod, curPosSnapped, xBounds, yBounds);

		for (int32_t y = yBounds.x; y < yBounds.y; y++)
			for (int32_t x = xBounds.x; x < xBounds.y; x++)
			{
				size_t chunkId = getChunkID(x, y, lod);

				m_LoadedNodes.insert(chunkId);

				m_Deserializer->loadChunkSequential(m_TerrainData->getChunkProperty(chunkId));
				if (m_Deserializer->loadedChunksInBuffer() == MAX_CHUNKS_LOADING_PER_FRAME)
				{
					m_Map->batchCopyBuffer(currentCommandBuffer, *m_Deserializer->getImageData(), m_Deserializer->getRegions());
					m_Deserializer->Flush();

					VulkanUtils::endSingleTimeCommand(currentCommandBuffer);
					currentCommandBuffer = VulkanUtils::beginSingleTimeCommand();
				}
			}
	}

	if (m_Deserializer->loadedChunksInBuffer() != 0)
	{
		m_Map->batchCopyBuffer(currentCommandBuffer, *m_Deserializer->getImageData(), m_Deserializer->getRegions());
		m_Deserializer->Flush();
	}

	prepareForRendering(currentCommandBuffer);
	VulkanUtils::endSingleTimeCommand(currentCommandBuffer);
}

void TerrainClipmap::prepareForDeserialization(VkCommandBuffer cmdBuffer)
{
	TerrainInfo terrainInfo = m_TerrainData->getSpecification().Info;

	VkImageSubresourceRange imgSubresource{};
	imgSubresource.aspectMask = m_Map->getSpecification().Aspect;
	imgSubresource.layerCount = terrainInfo.LODCount;
	imgSubresource.levelCount = 1;
	imgSubresource.baseMipLevel = 0;
	VulkanUtils::transitionImageLayout(cmdBuffer, m_Map->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void TerrainClipmap::prepareForRendering(VkCommandBuffer cmdBuffer)
{
	TerrainInfo terrainInfo = m_TerrainData->getSpecification().Info;

	VkImageSubresourceRange imgSubresource{};
	imgSubresource.aspectMask = m_Map->getSpecification().Aspect;
	imgSubresource.layerCount = terrainInfo.LODCount;
	imgSubresource.levelCount = 1;
	imgSubresource.baseMipLevel = 0;
	VulkanUtils::transitionImageLayout(cmdBuffer, m_Map->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
}

void TerrainClipmap::blitNodes(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer, const std::vector<VkBufferImageCopy>& regions)
{
	m_Map->batchCopyBuffer(cmdBuffer, *StagingBuffer, regions);
}

uint32_t TerrainClipmap::updateClipmaps(VkCommandBuffer cmdBuffer)
{
	uint32_t updated = m_Deserializer->Refresh(cmdBuffer, this);
	if (updated)
		m_LastValidCameraPosition = m_Deserializer->getLastPosition();

	return updated;
}

void TerrainClipmap::getLODBounds(int32_t lod, const glm::ivec2& camPositionSnapped, glm::ivec2& xBounds, glm::ivec2& yBounds)
{
	TerrainInfo terrainInfo = m_TerrainData->getSpecification().Info;

	int32_t chunkSize = terrainInfo.ChunkSize * (1 << lod);

	glm::ivec2 terrainCameraPosition = camPositionSnapped;
	terrainCameraPosition.x = glm::max(terrainCameraPosition.x, chunkSize * (m_RingSize / 2));
	terrainCameraPosition.y = glm::max(terrainCameraPosition.y, chunkSize * (m_RingSize / 2));
	terrainCameraPosition.x = glm::min(terrainCameraPosition.x, terrainInfo.TerrainSize - chunkSize * (m_RingSize / 2));
	terrainCameraPosition.y = glm::min(terrainCameraPosition.y, terrainInfo.TerrainSize - chunkSize * (m_RingSize / 2));

	glm::ivec2 snapedPosition = terrainCameraPosition / chunkSize;

	yBounds.x = glm::max(snapedPosition.y - m_RingSize / 2, 0);
	yBounds.y = glm::min(snapedPosition.y + m_RingSize / 2, terrainInfo.TerrainSize / chunkSize);

	xBounds.x = glm::max(snapedPosition.x - m_RingSize / 2, 0);
	xBounds.y = glm::min(snapedPosition.x + m_RingSize / 2, terrainInfo.TerrainSize / chunkSize);

	if (snapedPosition.x % 2 != 0)
	{
		xBounds.x -= 1;
		xBounds.y -= 1;
	}

	if (snapedPosition.y % 2 != 0)
	{
		yBounds.x -= 1;
		yBounds.y -= 1;
	}
}
