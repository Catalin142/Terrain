#pragma once

#include "TerrainChunk.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// 128 = 64m
struct TerrainInfo
{
	int32_t TerrainSize;

	int32_t padding_1;
	
	glm::vec2 ElevationRange = { -100.0f, 100.0f };
	int32_t ChunkSize = 128;
	uint32_t LODCount = 5;
	
	int32_t padding_2[2];
};

struct TerrainSpecification
{
	TerrainFileLocation ChunkedFilepath;
	std::string TerrainFilepath;

	TerrainInfo Info;
};

class TerrainData
{
public:
	TerrainData(const TerrainSpecification& spec);
	~TerrainData() = default;

	TerrainInfo getInfo() { return m_Specification.Info; }
	TerrainSpecification getSpecification() { return m_Specification; }

	uint32_t getSectorCount() { return uint32_t(m_Specification.Info.TerrainSize) / m_Specification.Info.ChunkSize; }
	void setElevationRange(float lower, float upper) { m_Specification.Info.ElevationRange = { lower, upper }; }

	void addChunkProperty(size_t chunk, const FileChunkProperties& props);
	const FileChunkProperties& getChunkProperty(size_t chunk);

	void updateInfo();

public:
	std::shared_ptr<VulkanBuffer> TerrainInfoBuffer;

private:
	TerrainSpecification m_Specification;
	std::unordered_map<size_t, FileChunkProperties> m_ChunkProperties;
};