#include "QuadTreeLOD.h"
#include <cmath>

QuadTreeLOD::QuadTreeLOD(const TerrainSpecification& spec)
{
	// how many times i divide the terrain size in order to get to the minimum lod
	uint32_t maxLod = 1;
	uint32_t currentSize = (uint32_t)spec.Info.TerrainSize.x / 2;
	while (currentSize > spec.Info.MinimumChunkSize)
	{
		currentSize /= 2;
		maxLod *= 2;
	}

	TerrainQuadTreeProperties terrainQuadProps;
	terrainQuadProps.Size = spec.Info.TerrainSize;
	terrainQuadProps.MaxLod = maxLod;
	terrainQuadProps.LodDistance = 64;
	m_TerrainQuadTree = std::make_shared<TerrainQuadTree>(terrainQuadProps);
}

void QuadTreeLOD::getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition)
{
	m_TerrainQuadTree->insertPlayer({ cameraPosition.x, cameraPosition.z });

	chunks = m_TerrainQuadTree->getLeaves();
}
