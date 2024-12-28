#include "QuadTreeLOD.h"
#include <cmath>

QuadTreeLOD::QuadTreeLOD(const TerrainSpecification& spec)
{
	TerrainQuadTreeProperties terrainQuadProps;
	terrainQuadProps.Size = spec.Info.TerrainSize;
	terrainQuadProps.MaxLod = spec.Info.LodCount;
	terrainQuadProps.LodDistance = 64;
	m_TerrainQuadTree = std::make_shared<TerrainQuadTree>(terrainQuadProps);
}

void QuadTreeLOD::getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition)
{
	m_TerrainQuadTree->insertPlayer({ cameraPosition.x, cameraPosition.z });
	chunks = m_TerrainQuadTree->getLeaves();
}

const std::vector<TerrainChunk>& QuadTreeLOD::getVisitedNodes()
{
	return m_TerrainQuadTree->getVisitedNodes();
}
