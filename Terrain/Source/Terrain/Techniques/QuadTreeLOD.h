#pragma once

#include "Terrain/Terrain.h"

class QuadTreeLOD
{
public:
	QuadTreeLOD(const TerrainSpecification& spec);
	~QuadTreeLOD() = default;

	void getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition);
	const std::vector<TerrainChunk>& getVisitedNodes();

private:
	TerrainSpecification m_TerrainSpecification;
};