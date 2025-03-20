#include "Terrain.h"

#include "Techniques/QuadTreeLOD.h"

TerrainData::TerrainData(const TerrainSpecification& spec) : m_Specification(spec)
{ 
    std::ifstream tabCache = std::ifstream(m_Specification.ChunkedFilepath.Table);
    uint32_t size, mip, worldOffset;
    size_t binOffset;

    while (tabCache >> mip >> worldOffset >> binOffset)
        m_ChunkProperties[getChunkID(worldOffset, mip)] = { worldOffset, mip, binOffset, 130 * 130 * 2 };


	{
		VulkanBufferProperties terrainInfoProperties;
		terrainInfoProperties.Size = ((uint32_t)sizeof(TerrainInfo));
		terrainInfoProperties.Type = BufferType::UNIFORM_BUFFER;
		terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		TerrainInfoBuffer = std::make_shared<VulkanBuffer>(terrainInfoProperties);
	}
}

void TerrainData::addChunkProperty(size_t chunk, const FileChunkProperties& props)
{
	m_ChunkProperties[chunk] = props;
}

const FileChunkProperties& TerrainData::getChunkProperty(size_t chunk)
{
	assert(m_ChunkProperties.find(chunk) != m_ChunkProperties.end());
	return m_ChunkProperties.at(chunk);
}

void TerrainData::updateInfo()
{
	TerrainInfoBuffer->setDataCPU(&m_Specification.Info, sizeof(TerrainInfo));
}
