#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/Container/TerrainQuadTree.h"

class SinkingLOD
{
public:
	SinkingLOD(const TerrainSpecification& spec);
	~SinkingLOD() = default;

	void getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition);

private:
	TerrainSpecification m_TerrainSpecification;
};