#include "SinkingLOD.h"

#include "glm/glm.hpp"

SinkingLOD::SinkingLOD(const TerrainSpecification& spec) : m_TerrainSpecification(spec)
{ }

void SinkingLOD::getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition)
{
	/*int32_t chunkSize = (uint32_t)m_TerrainSpecification.Info.MinimumChunkSize;
	while (chunkSize <= m_TerrainSpecification.Info.TerrainSize.x)
	{
		TerrainChunk newChunk;
		newChunk.Size = chunkSize;
		newChunk.Lod = 1;
		newChunk.Offset.x = glm::clamp(cameraPosition.x - float(chunkSize / 2), 0.0f, m_TerrainSpecification.Info.TerrainSize.x - chunkSize);
		newChunk.Offset.y = glm::clamp(cameraPosition.z - float(chunkSize / 2), 0.0f, m_TerrainSpecification.Info.TerrainSize.x - chunkSize);
		
		chunks.push_back(newChunk);
		chunkSize *= 2;
	}*/

}
