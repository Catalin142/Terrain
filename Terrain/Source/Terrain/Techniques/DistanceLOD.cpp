#include "DistanceLOD.h"

#include <algorithm>

DistanceLOD::DistanceLOD(TerrainSpecification& spec, const std::vector<LODRange>& LODRanges) : m_TerrainSpecification(spec),
m_LODRanges(LODRanges)
{
	std::sort(m_LODRanges.begin(), m_LODRanges.end(), [&](const LODRange& lhs, const LODRange& rhs) {
		return lhs.DistanceNear < rhs.DistanceNear;
		});

	Initialize();
}

void DistanceLOD::getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition)
{
	uint32_t minimumChunkSize = m_TerrainSpecification.Info.MinimumChunkSize;
	uint32_t chunksPerRow = (uint32_t)m_TerrainSpecification.Info.TerrainSize.x / minimumChunkSize;
	uint32_t currentLOD = 0;

	/*for (LODRange& range : m_LODRanges)
	{
		range.CurrentCount = 0;
		for (auto& currentChunk : m_Chunks)
		{
			float distance = glm::distance(glm::vec3(cameraPosition.x, glm::min(0.0f, cameraPosition.y), cameraPosition.z),
				glm::vec3(currentChunk.Offset.x, 0.0f, currentChunk.Offset.y));
			if (distance >= range.DistanceNear && distance < range.DistanceFar)
			{
				m_LodMap[(uint32_t(currentChunk.Offset.y) / minimumChunkSize) * chunksPerRow + (uint32_t(currentChunk.Offset.x) / minimumChunkSize)].LOD = range.LOD;
				range.CurrentCount++;
				chunks.push_back({ currentChunk.Offset, range.LOD });
			}
		}
		currentLOD++;
	}*/
}

void DistanceLOD::Initialize()
{
	uint32_t chunkSize = m_TerrainSpecification.Info.MinimumChunkSize;

	m_LodMap.resize(uint32_t(m_TerrainSpecification.Info.TerrainSize.x) / chunkSize *
		uint32_t(m_TerrainSpecification.Info.TerrainSize.y) / chunkSize, { -1 });

	uint32_t gridSizeX = (uint32_t)m_TerrainSpecification.Info.TerrainSize.x;
	uint32_t gridSizeY = (uint32_t)m_TerrainSpecification.Info.TerrainSize.y;

	/*for (uint32_t xOffset = 0; xOffset < gridSizeX / chunkSize; xOffset++)
		for (uint32_t yOffset = 0; yOffset < gridSizeY / chunkSize; yOffset++)
		{
			TerrainChunk& chunk = m_Chunks.emplace_back();
			chunk.Offset.x = float(xOffset * chunkSize);
			chunk.Offset.y = float(yOffset * chunkSize);
		}*/
}
