#include "Terrain.h"

#include "Techniques/DistanceLOD.h"
#include "Techniques/QuadTreeLOD.h"
#include "Techniques/SinkingLOD.h"

Terrain::Terrain(const TerrainSpecification& spec) : m_Specification(spec)
{
	setTechnique(m_CurrentLODTechnique);
}

const std::vector<TerrainChunk>& Terrain::getChunksToRender(const glm::vec3& cameraPosition)
{
	m_ChunksToRender.clear();

	switch (m_CurrentLODTechnique)
	{
	case LODTechnique::DISTANCE_BASED:
		m_DistanceLOD->getChunksToRender(m_ChunksToRender, cameraPosition);
		break;

	case LODTechnique::QUAD_TREE:
		m_QuadTreeLOD->getChunksToRender(m_ChunksToRender, cameraPosition);
		break;

	case LODTechnique::SINKING_CIRCLE:
		m_SinkingCircleLOD->getChunksToRender(m_ChunksToRender, cameraPosition);
	}

	return m_ChunksToRender;
}

void Terrain::setTechnique(LODTechnique tech)
{
	m_CurrentLODTechnique = tech;
	
	std::vector<LODRange> ranges{
		{ 0.0f, 300.0f, 1},
		{ 300.0f, 800.0f, 2},
		{ 800.0f, 1200.0f, 4},
		{ 1200.0f, FLT_MAX, 8},
	};

	switch (m_CurrentLODTechnique)
	{
	case LODTechnique::DISTANCE_BASED:

		m_QuadTreeLOD.reset();
		m_DistanceLOD = std::make_shared<DistanceLOD>(m_Specification, ranges);

		break;

	case LODTechnique::QUAD_TREE:

		m_DistanceLOD.reset();
		m_QuadTreeLOD = std::make_shared<QuadTreeLOD>(m_Specification);

		break;

	case LODTechnique::SINKING_CIRCLE:

		m_SinkingCircleLOD.reset();
		m_SinkingCircleLOD = std::make_shared<SinkingLOD>(m_Specification);

		break;
	}
}
