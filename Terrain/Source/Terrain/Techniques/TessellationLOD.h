#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/Clipmap/TerrainClipmap.h"

#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Graphics/Vulkan/VulkanPass.h"

#include "Scene/Camera.h"

#include "glm/glm.hpp"

#include <memory>

class TessellationLOD
{
public:
	TessellationLOD(const TerrainSpecification& spec, const std::shared_ptr<TerrainClipmap>& clipmap);
	~TessellationLOD() = default;

	void createResources(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<VulkanBuffer>& tessSettings);

	void computeMargins(const glm::ivec2& cameraPosition, uint32_t patchSize);
	void Generate(VkCommandBuffer commandBuffer, const Camera& cam, uint32_t patchSize);

	uint32_t getMostRecentIndex() { return m_CurrentlyUsedBuffer; }

public:
	std::shared_ptr<VulkanBuffer> ChunksToRender;
	std::shared_ptr<VulkanBufferSet> LODMarginsBufferSet;
	std::shared_ptr<VulkanBuffer> RenderCommand;

private:
	TerrainSpecification m_TerrainSpecification;

	VulkanComputePass m_ConstructTerrainChunksPass;
	std::shared_ptr<VulkanBuffer> m_FrustumBuffer;

	int32_t m_RingSize;

	uint32_t m_CurrentlyUsedBuffer = 0;
	uint32_t m_NextBuffer = 0;
};
