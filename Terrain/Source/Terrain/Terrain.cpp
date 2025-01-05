#include "Terrain.h"

#include "Techniques/QuadTreeLOD.h"

Terrain::Terrain(const TerrainSpecification& spec) : m_Specification(spec)
{
	setTechnique(m_CurrentLODTechnique);
}

const std::vector<TerrainChunk>& Terrain::getChunksToRender(const glm::vec3& cameraPosition)
{
	return ctr;

	m_ChunksToRender.clear();

	switch (m_CurrentLODTechnique)
	{
	case LODTechnique::QUAD_TREE:
		m_QuadTreeLOD->getChunksToRender(m_ChunksToRender, cameraPosition);
		break;
	}

	return m_ChunksToRender;
}

const std::vector<TerrainChunk>& Terrain::getQuadTreeVisitedNodes()
{
	switch (m_CurrentLODTechnique)
	{
	case LODTechnique::QUAD_TREE:
		return m_QuadTreeLOD->getVisitedNodes();
		break;

	case LODTechnique::DISTANCE_BASED:
	case LODTechnique::SINKING_CIRCLE:
		return std::vector<TerrainChunk>();
	}
}

void Terrain::setTechnique(LODTechnique tech)
{
	m_CurrentLODTechnique = tech;
	switch (m_CurrentLODTechnique)
	{
	case LODTechnique::QUAD_TREE:

		m_DistanceLOD.reset();
		m_QuadTreeLOD = std::make_shared<QuadTreeLOD>(m_Specification);

		break;
	}
}

void Terrain::Update(VkCommandBuffer cmdBuffer)
{
}
