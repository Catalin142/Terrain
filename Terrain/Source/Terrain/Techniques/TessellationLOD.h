#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/Clipmap/TerrainClipmap.h"

#include "Graphics/Vulkan/VulkanBuffer.h"

#include "glm/glm.hpp"

#include <memory>

class TessellationLOD
{
public:
	TessellationLOD(const TerrainSpecification& spec, const std::shared_ptr<TerrainClipmap>& clipmap);
	~TessellationLOD() = default;

	uint32_t Generate(const glm::ivec2& cameraPosition);
	uint32_t getMostRecentIndex() { return m_CurrentlyUsedBuffer; }

public:
	std::shared_ptr<VulkanBufferSet> chunksToRender;
	std::shared_ptr<VulkanBufferSet> LODMarginsBufferSet;

private:
	TerrainSpecification m_TerrainSpecification;
	int32_t m_RingSize;

	uint32_t m_CurrentlyUsedBuffer = 0;
	uint32_t m_NextBuffer = 0;
};
