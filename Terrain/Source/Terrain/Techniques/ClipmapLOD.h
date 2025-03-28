#pragma once

#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Graphics/Vulkan/VulkanPass.h"

#include "Terrain/Terrain.h"
#include "Terrain/Clipmap/TerrainClipmap.h"

#include "Scene/Camera.h"

#include <memory>
#include <vulkan/vulkan.h>

class ClipmapLOD
{
public:
	ClipmapLOD(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<TerrainClipmap>& clipmap);
	~ClipmapLOD() = default;

	// returns count of chunks to render
	void computeMargins(const glm::ivec2& cameraPosition);
	void Generate(VkCommandBuffer commandBuffer, const Camera& cam);
	uint32_t getMostRecentIndex() { return m_CurrentlyUsedBuffer; }

private:
	void createChunkComputePass(const std::shared_ptr<VulkanBuffer>& infoBuffer);

public:
	std::shared_ptr<VulkanBuffer> ChunksToRender;
	std::shared_ptr<VulkanBuffer> m_RenderCommand;
	std::shared_ptr<VulkanBufferSet> LODMarginsBufferSet;

private:
	TerrainSpecification m_TerrainSpecification;
	int32_t m_RingSize;

	VulkanComputePass m_ConstructTerrainChunksPass;
	std::shared_ptr<VulkanBuffer> m_FrustumBuffer;

	uint32_t m_CurrentlyUsedBuffer = 0;
	uint32_t m_NextBuffer = 0;
};