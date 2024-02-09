#include "DistanceLOD.h"

#include <algorithm>

DistanceLOD::DistanceLOD(TerrainSpecification& spec, const std::vector<LODRange>& LODRanges) : m_TerrainSpecification(spec),
m_LODRanges(LODRanges)
{
	std::sort(m_LODRanges.begin(), m_LODRanges.end(), [&](const LODRange& lhs, const LODRange& rhs) {
		return lhs.DistanceNear < rhs.DistanceNear;
		});

	m_LodMap.resize(16 * 16, { -1 });

	Initialize();
}

void DistanceLOD::getChunksToRender(std::vector<TerrainChunk>& chunks, const glm::vec3& cameraPosition)
{
	uint32_t minimumChunkSize = m_TerrainSpecification.MinimumChunkSize;
	uint32_t chunksPerRow = m_TerrainSpecification.TerrainSize.x / minimumChunkSize;
	uint32_t currentLOD = 0;

	for (const LODRange& range : m_LODRanges)
	{
		m_LODProperties[currentLOD].CurrentCount = 0;
		for (auto& currentChunk : m_Chunks)
		{
			float distance = glm::distance(cameraPosition, glm::vec3(currentChunk.Offset.x, 0.0f, currentChunk.Offset.y));
			if (distance >= range.DistanceNear && distance < range.DistanceFar)
			{
				// TODO: Get rid of hardcoded 16
				m_LodMap[(currentChunk.Offset.y / minimumChunkSize) * 16 + (currentChunk.Offset.x / minimumChunkSize)].LOD = range.LOD;
				m_LODProperties[currentLOD].CurrentCount++;
				chunks.push_back({ currentChunk.Offset, minimumChunkSize, 0 });
			}
		}
		currentLOD++;
	}
}

void DistanceLOD::Initialize()
{
	loadTextures();

	uint32_t chunkSize = m_TerrainSpecification.MinimumChunkSize;

	uint32_t gridSizeX = (uint32_t)m_TerrainSpecification.TerrainSize.x;
	uint32_t gridSizeY = (uint32_t)m_TerrainSpecification.TerrainSize.y;

	for (uint32_t xOffset = 0; xOffset < gridSizeX / chunkSize; xOffset++)
		for (uint32_t yOffset = 0; yOffset < gridSizeY / chunkSize; yOffset++)
		{
			TerrainChunk& chunk = m_Chunks.emplace_back();
			chunk.Offset.x = xOffset * chunkSize;
			chunk.Offset.y = yOffset * chunkSize;
		}

	for (LODRange& lod : m_LODRanges)
	{
		uint32_t vertCount = chunkSize + 1;
		std::vector<uint32_t> indices = generateIndices(lod.LOD, vertCount);

		LODProperties& prop = m_LODProperties.emplace_back();

		prop.IndicesCount = indices.size();
		prop.IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
			BufferType::INDEX, BufferUsage::STATIC);
	}
}

void DistanceLOD::loadTextures()
{
	{
		TextureSpecification heightMapSpec{};
		heightMapSpec.CreateSampler = true;
		heightMapSpec.Filepath.push_back(m_TerrainSpecification.HeightMap);
		m_HeightMap = std::make_shared<VulkanTexture>(heightMapSpec);

		m_TerrainSpecification.TerrainSize = { (float)m_HeightMap->getInfo().Width, (float)m_HeightMap->getInfo().Height };
	}
	{
		TextureSpecification compositionMapSpec{};
		compositionMapSpec.CreateSampler = true;
		compositionMapSpec.Channles = 4;
		compositionMapSpec.Filepath.push_back(m_TerrainSpecification.CompositionMap);
		m_CompositionMap = std::make_shared<VulkanTexture>(compositionMapSpec);
	}
	{
		{
			TextureSpecification texSpec{};
			texSpec.CreateSampler = true;
			texSpec.GenerateMips = true;
			texSpec.Channles = 4;
			texSpec.LayerCount = m_TerrainSpecification.TerrainTextures.size();
			texSpec.Filepath = m_TerrainSpecification.TerrainTextures;
			m_TerrainTextures = std::make_shared<VulkanTexture>(texSpec);
		}
	}
}

std::vector<uint32_t> DistanceLOD::generateIndices(uint32_t lod, uint32_t vertCount)
{
	std::vector<uint32_t> indices;

	for (uint32_t x = 0; x < vertCount - 1; x += lod * 2) {
		for (uint32_t y = 0; y < vertCount - 1; y += lod * 2) {
			uint32_t index = x * vertCount + y;
			/*
			c      d      i


			b      e      h


			a      f      g
			*/

			uint32_t a = index;
			uint32_t b = index + lod;
			uint32_t c = index + lod * 2;
			uint32_t d = index + vertCount * lod + lod * 2;
			uint32_t e = index + vertCount * lod + lod;
			uint32_t f = index + vertCount * lod;
			uint32_t g = index + vertCount * lod * 2;
			uint32_t h = index + vertCount * lod * 2 + lod;
			uint32_t i = index + vertCount * lod * 2 + lod * 2;

			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(e);

			indices.push_back(f);
			indices.push_back(a);
			indices.push_back(e);

			indices.push_back(b);
			indices.push_back(c);
			indices.push_back(e);

			indices.push_back(e);
			indices.push_back(c);
			indices.push_back(d);

			indices.push_back(e);
			indices.push_back(d);
			indices.push_back(i);

			indices.push_back(h);
			indices.push_back(e);
			indices.push_back(i);

			indices.push_back(f);
			indices.push_back(e);
			indices.push_back(g);

			indices.push_back(g);
			indices.push_back(e);
			indices.push_back(h);
		}
	}

	return indices;
}
