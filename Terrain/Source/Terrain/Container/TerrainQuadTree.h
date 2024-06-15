#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

#include "glm/glm.hpp"
#include "Terrain/TerrainChunk.h"

class TerrainQuadTreeNode
{
public:
	TerrainQuadTreeNode();

	void Divide(std::vector<TerrainQuadTreeNode*> nodePool, uint32_t& currentPoolIndex);

	TerrainChunk getChildren(uint8_t i);

public:
	TerrainChunk Load;

private:
	TerrainChunk m_Children[4];
};

struct TerrainQuadTreeProperties
{
	glm::uvec2 Size;
	uint8_t MaxLod = 6;
	int32_t LodDistance = 100;
};


class TerrainQuadTree
{
public:
	TerrainQuadTree(const TerrainQuadTreeProperties& props);
	~TerrainQuadTree();

	TerrainQuadTreeNode* getRoot() { return m_Root; }

	void insertPlayer(const glm::vec2& playerPosition);
	const std::vector<TerrainChunk>& getLeaves() { return m_Leaves; }
	const std::vector<TerrainChunk>& getVisitedNodes() { return m_VisitedNodes; }

private:
	//void insertPlayer(TerrainQuadTreeNode* node, const glm::uvec2& playerPosition, uint8_t currentLod);

private:
	TerrainQuadTreeProperties m_Properties;
	std::vector<TerrainChunk> m_Leaves;
	std::vector<TerrainChunk> m_VisitedNodes;

	const uint32_t m_PoolSize = 1024;
	std::vector<TerrainQuadTreeNode*> m_NodePool;
	TerrainQuadTreeNode* m_Root;
};