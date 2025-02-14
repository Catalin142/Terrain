#include "Terrain.h"

#include "Techniques/QuadTreeLOD.h"

TerrainData::TerrainData(const TerrainSpecification& spec) : m_Specification(spec)
{ 
    std::ifstream tabCache = std::ifstream(m_Specification.Filepath.Table);
    uint32_t size, mip, worldOffset;
    size_t binOffset;

    while (tabCache >> size >> mip >> worldOffset >> binOffset)
        m_ChunkProperties[getChunkID(worldOffset, mip)] = { worldOffset, mip, binOffset, size };
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