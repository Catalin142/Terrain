#include "TerrainQuadTree.h"

#include <queue>

TerrainQuadTreeNode::TerrainQuadTreeNode()
{
}

TerrainChunk TerrainQuadTreeNode::getChildren(uint8_t i)
{
	//if (i < 0 || i > 3) assert(false);
	return m_Children[i];
}

void TerrainQuadTreeNode::Divide(std::vector<TerrainQuadTreeNode*> nodePool, uint32_t& currentPoolIndex)
{
	//uint32_t childrenSize = Load.Size / 2u;

	//glm::uvec2 childrenPositions[4] =
	//{
	//	glm::uvec2(Load.Offset.x, Load.Offset.y), // bl
	//	glm::uvec2(Load.Offset.x + Load.Size / 2u, Load.Offset.y), // br
	//	glm::uvec2(Load.Offset.x, Load.Offset.y + Load.Size / 2u), // tl
	//	glm::uvec2(Load.Offset.x + Load.Size / 2u, Load.Offset.y + Load.Size / 2u) // tr
	//};

	//for (int i = 0; i < 4; i++)
	//{
	//	m_Children[i].Size = childrenSize;
	//	m_Children[i].Offset = childrenPositions[i];
	//	m_Children[i].Lod = Load.Lod / 2;
	//}
}


TerrainQuadTree::TerrainQuadTree(const TerrainQuadTreeProperties& props)
{
	//m_Properties = props;

	//m_Leaves.reserve(128);

	//m_NodePool.resize(m_PoolSize);
	//for (uint32_t index = 0; index < m_PoolSize; index++)
	//	m_NodePool[index] = new TerrainQuadTreeNode();

	//m_Root = new TerrainQuadTreeNode();

	//m_Root->Load.Offset = { 0.0, 0.0 }; // Position is on bottom left corner
	//m_Root->Load.Size = m_Properties.Size.x;
	//m_Root->Load.Lod = m_Properties.MaxLod * 2;
}

TerrainQuadTree::~TerrainQuadTree()
{
	//for (uint32_t index = 0; index < m_PoolSize; index++)
	//	delete m_NodePool[index];
	//delete m_Root;
}

//static bool containtsPlayer(const glm::ivec2& nodePosition, int32_t nodeSize, const glm::ivec2& playerPosition)
//{
//	return (playerPosition.x >= nodePosition.x && playerPosition.x <= nodePosition.x + nodeSize &&
//		playerPosition.y >= nodePosition.y && playerPosition.y <= nodePosition.y + nodeSize);
//}
//
//static bool isClose(const glm::ivec2& nodePosition, int32_t nodeSize, const glm::ivec2& playerPosition, int32_t minDistance)
//{
//	return  containtsPlayer(nodePosition, nodeSize, playerPosition + glm::ivec2(minDistance, 0u)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition - glm::ivec2(minDistance, 0u)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition + glm::ivec2(0u, minDistance)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition - glm::ivec2(0u, minDistance)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition + glm::ivec2(minDistance, minDistance)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition - glm::ivec2(minDistance, minDistance)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition + glm::ivec2(-minDistance, minDistance)) ||
//		containtsPlayer(nodePosition, nodeSize, playerPosition + glm::ivec2(minDistance, -minDistance));
//}

void TerrainQuadTree::insertPlayer(const glm::vec2& playerPosition)
{
	/*m_Leaves.clear();
	m_VisitedNodes.clear();

	int32_t lastPos = 1;

	m_NodePool[0]->Load = m_Root->Load;

	for (int32_t index = 0; index < (int32_t)m_PoolSize; index++)
	{
		TerrainQuadTreeNode* currentNode = m_NodePool[index];

		TerrainChunk nodeLoad = currentNode->Load;

		if (nodeLoad.Lod <= 0 || lastPos <= index)
			break;

		glm::ivec2 playerPos = playerPosition;
		bool divide = containtsPlayer(glm::ivec2(nodeLoad.Offset), nodeLoad.Size, playerPos) ||
			isClose(glm::ivec2(nodeLoad.Offset), nodeLoad.Size, playerPos, m_Properties.LodDistance);

		m_VisitedNodes.push_back(currentNode->Load);
		if (!divide || nodeLoad.Lod == 1)
		{
			m_Leaves.push_back(currentNode->Load);
			continue;
		}
		
		uint32_t fakePos = lastPos;
		currentNode->Divide(m_NodePool, fakePos);

		m_NodePool[lastPos++]->Load = currentNode->getChildren(1);
		m_NodePool[lastPos++]->Load = currentNode->getChildren(2);
		m_NodePool[lastPos++]->Load = currentNode->getChildren(3);
		m_NodePool[index]->Load = currentNode->getChildren(0);

		currentNode = m_NodePool[index--];
	}*/

	//while (!BfsQueue.empty())
	//{
	//	size_t queueSize = BfsQueue.size();

	//	for (size_t i = 0; i < queueSize; i++)
	//	{
	//		TerrainQuadTreeNode* currentNode = BfsQueue.front();
	//		BfsQueue.pop();

	//		TerrainChunk nodeLoad = currentNode->Load;
	//		currentNode->Load.Lod = currentLod;

	//		bool divide = containtsPlayer(nodeLoad.Offset, nodeLoad.Size, playerPosition) ||
	//					  isClose(nodeLoad.Offset, nodeLoad.Size, playerPosition, (uint32_t)m_Properties.LodDistance);

	//		if (!divide || currentLod <= 1)
	//		{
	//			m_Leaves.push_back(currentNode->Load);
	//			continue;
	//		}

	//		currentNode->Divide(m_NodePool, currentPoolIndex);

	//		BfsQueue.push(currentNode->getChildren(0));
	//		BfsQueue.push(currentNode->getChildren(1));
	//		BfsQueue.push(currentNode->getChildren(2));
	//		BfsQueue.push(currentNode->getChildren(3));
	//	}

	//	currentLod /= 2;
	//}
}