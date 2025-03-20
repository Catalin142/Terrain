#pragma once

#include "Graphics/Vulkan/VulkanPass.h"
#include "Graphics/Vulkan/VulkanBuffer.h"

#include "Terrain/Terrain.h"

#include "Scene/Camera.h"

#include <memory>

class BruteForceLOD
{
public:
	BruteForceLOD(const TerrainSpecification& spec);
	~BruteForceLOD() = default;

	void Generate(VkCommandBuffer commandBuffer, const Camera& cam, const glm::vec4& distanceThreshold);
	const std::shared_ptr<VulkanBuffer>& getIndirectDrawCommand();

	uint32_t getMostRecentIndex() { return m_CurrentlyUsedBuffer; }

private:
	void createComputePass();

public:
	std::shared_ptr<VulkanBuffer> ChunksToRender;
	std::shared_ptr<VulkanBufferSet> ResultCount;
	glm::vec4 DistanceThreshold;

private:
	VulkanComputePass m_ConstructTerrainChunksPass;
	std::shared_ptr<VulkanBuffer> m_ComputePassBufferData;

	std::shared_ptr<VulkanBuffer> m_FrustumBuffer;
	std::shared_ptr<VulkanBufferSet> m_DrawIndirectCommandsSet;

	TerrainSpecification m_TerrainSpecification;

	uint32_t m_CurrentlyUsedBuffer = 0;
	uint32_t m_NextBuffer = 0;
};