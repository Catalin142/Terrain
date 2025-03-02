#include "TessellationLOD.h"

#include "Graphics/Vulkan/VulkanRenderer.h"

TessellationLOD::TessellationLOD(const TerrainSpecification& spec, const std::shared_ptr<TerrainClipmap>& clipmap)
{
	ClipmapTerrainSpecification clipmapSpec = clipmap->getSpecification();
	m_RingSize = clipmapSpec.ClipmapSize / spec.Info.ChunkSize;

	{
		VulkanBufferProperties chunkToRenderProperties;
		chunkToRenderProperties.Size = 1024 * (uint32_t)sizeof(TerrainChunk);
		chunkToRenderProperties.Type = BufferType::STORAGE_BUFFER;
		chunkToRenderProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		chunksToRender = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), chunkToRenderProperties);
	}

	{
		VulkanBufferProperties LODMarginsProperties;
		LODMarginsProperties.Size = m_TerrainSpecification.Info.LODCount * ((uint32_t)sizeof(TerrainInfo));
		LODMarginsProperties.Type = BufferType::STORAGE_BUFFER;
		LODMarginsProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		LODMarginsBufferSet = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), LODMarginsProperties);
	}
}

uint32_t TessellationLOD::Generate(const glm::ivec2& cameraPosition)
{

	std::vector<TerrainChunk> chunks;
	std::vector<LODMargins> margins;

	int32_t prevminY;
	int32_t prevmaxY;
	int32_t prevminX;
	int32_t prevmaxX;

	TerrainInfo tInfo = m_TerrainSpecification.Info;

	for (int32_t lod = 0; lod < tInfo.LODCount; lod++)
	{
		TerrainChunk tc;
		tc.Lod = lod;

		int32_t chunkSize = tInfo.ChunkSize * (1 << lod);

		glm::ivec2 newCamPos = cameraPosition;
		newCamPos.x = glm::max(newCamPos.x, chunkSize * (m_RingSize / 2));
		newCamPos.y = glm::max(newCamPos.y, chunkSize * (m_RingSize / 2));
		newCamPos.x = glm::min(newCamPos.x, tInfo.TerrainSize - chunkSize * (m_RingSize / 2));
		newCamPos.y = glm::min(newCamPos.y, tInfo.TerrainSize - chunkSize * (m_RingSize / 2));

		glm::ivec2 snapedPosition;
		snapedPosition.x = int32_t(newCamPos.x) / chunkSize;
		snapedPosition.y = int32_t(newCamPos.y) / chunkSize;

		int32_t minY = glm::max(int32_t(snapedPosition.y) - m_RingSize / 2, 0);
		int32_t maxY = glm::min(int32_t(snapedPosition.y) + m_RingSize / 2, tInfo.TerrainSize / chunkSize);

		int32_t minX = glm::max(int32_t(snapedPosition.x) - m_RingSize / 2, 0);
		int32_t maxX = glm::min(int32_t(snapedPosition.x) + m_RingSize / 2, tInfo.TerrainSize / chunkSize);

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

		margins.push_back(LODMargins{ { minX, maxX }, { minY, maxY } });

		for (int32_t y = minY; y < maxY; y++)
			for (int32_t x = minX; x < maxX; x++)
			{
				tc.Offset = packOffset(x, y);

				if (lod != 0)
				{
					bool add = true;

					uint32_t marginminY = prevminY / 2;
					uint32_t marginmaxY = prevmaxY / 2;
					uint32_t marginminX = prevminX / 2;
					uint32_t marginmaxX = prevmaxX / 2;

					if (x >= marginminX && x < marginmaxX && y >= marginminY && y < marginmaxY)
						add = false;

					if (add)
						chunks.push_back(tc);
				}
				else
					chunks.push_back(tc);
			}

		prevminY = minY;
		prevmaxY = maxY;
		prevminX = minX;
		prevmaxX = maxX;
	}

	chunksToRender->getBuffer(m_NextBuffer)->setDataCPU(chunks.data(), chunks.size() * sizeof(TerrainChunk));
	LODMarginsBufferSet->getBuffer(m_NextBuffer)->setDataCPU(margins.data(), margins.size() * sizeof(LODMargins));

	m_CurrentlyUsedBuffer = m_NextBuffer;
	(++m_NextBuffer) %= VulkanRenderer::getFramesInFlight();

	return (uint32_t)chunks.size();
}
