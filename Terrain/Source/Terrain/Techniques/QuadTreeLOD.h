#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/Container/TerrainQuadTree.h"

class QuadTreeLOD
{
public:
	QuadTreeLOD(const TerrainSpecification& spec);
	~QuadTreeLOD() = default;

	void getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition);

private:
	TerrainSpecification m_TerrainSpecification;
	std::shared_ptr<TerrainQuadTree> m_TerrainQuadTree;
};