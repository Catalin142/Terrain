#include "Terrain.h"

#include "Techniques/DistanceLOD.h"

Terrain::Terrain(const TerrainSpecification& spec) : m_Specification(spec)
{
	std::vector<LODRange> ranges{
		{ 0.0f, 450.0f, 4},
		{ 450.0f, 800.0f, 8},
		{ 800.0f, 1200.0f, 16},
		{ 1200.0f, FLT_MAX, 32},
	};

	m_DistanceLOD = std::make_shared<DistanceLOD>(m_Specification, ranges);
}

const std::vector<TerrainChunk>& Terrain::getChunksToRender(const glm::vec3& cameraPosition)
{
	m_ChunksToRender.clear();

	switch (m_CurrentLODTechnique)
	{
	case LODTechnique::DISTANCE_BASED:
		m_DistanceLOD->getChunksToRender(m_ChunksToRender, cameraPosition);
		break;
	}

	return m_ChunksToRender;
}
