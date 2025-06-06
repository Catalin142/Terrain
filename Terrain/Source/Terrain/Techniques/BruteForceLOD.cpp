#include "BruteForceLOD.h"

#include "Terrain/BruteForce/BruteForceData.h"

#include "Graphics/Vulkan/VulkanUtils.h"

#define CONSTRUCT_TERRAIN_CHUNKS_BRUTE_FORCE_COMPUTE "Terrain/BruteForce/ConstructTerrainChunks_comp.glsl"

struct GPUComputePassInfo
{
	glm::vec3 CameraPosition;
	int32_t padding;

	glm::vec4 DistanceThreshold;
};

BruteForceLOD::BruteForceLOD(const std::unique_ptr<TerrainData>& terrain) : m_TerrainSpecification(terrain->getSpecification())
{
	createComputePass(terrain->TerrainInfoBuffer);
}

void BruteForceLOD::Generate(VkCommandBuffer commandBuffer, const Camera& cam, const glm::vec4& distanceThreshold)
{
	GPUComputePassInfo passInfo;
	passInfo.CameraPosition = cam.getPosition();
	passInfo.DistanceThreshold = distanceThreshold;

	m_ComputePassBufferData->setDataCPU(&passInfo, sizeof(GPUComputePassInfo));

	std::array<glm::vec4, 6> frustumPlanes = cam.getFrustum();
	m_FrustumBuffer->setDataCPU(&frustumPlanes, 6 * sizeof(glm::vec4));

	uint32_t count = 0;
	ResultCount->getBuffer(m_NextBuffer)->setDataCPU(&count, sizeof(uint32_t));

	uint32_t dispatchCount = m_TerrainSpecification.Info.TerrainSize / 128 / 16;

	m_ConstructTerrainChunksPass.Prepare(commandBuffer, m_NextBuffer);
	m_ConstructTerrainChunksPass.Dispatch(commandBuffer, { dispatchCount, dispatchCount, 1 });

	VulkanUtils::bufferMemoryBarrier(commandBuffer, ChunksToRender, { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT });

	VulkanUtils::bufferMemoryBarrier(commandBuffer, m_DrawIndirectCommandsSet->getBuffer(m_NextBuffer), { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });

	m_CurrentlyUsedBuffer = m_NextBuffer;
	(++m_NextBuffer) %= VulkanSwapchain::framesInFlight;
}

const std::shared_ptr<VulkanBuffer>& BruteForceLOD::getIndirectDrawCommand()
{
	return m_DrawIndirectCommandsSet->getBuffer(m_CurrentlyUsedBuffer);
}

void BruteForceLOD::createComputePass(const std::shared_ptr<VulkanBuffer>& infoBuffer)
{
	{
		VulkanBufferProperties indirectProperties;
		indirectProperties.Size = sizeof(VkDrawIndirectCommand);
		indirectProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		indirectProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_DrawIndirectCommandsSet = std::make_shared<VulkanBufferSet>(2, indirectProperties);
	}

	{
		VulkanBufferProperties frustumBufferProperties;
		frustumBufferProperties.Size = ((uint32_t)sizeof(glm::vec4)) * 6;
		frustumBufferProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		frustumBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_FrustumBuffer = std::make_shared<VulkanBuffer>(frustumBufferProperties);
	}

	{
		VulkanBufferProperties chunksToRenderProperties;
		chunksToRenderProperties.Size = 16384 * (uint32_t)sizeof(GPUBruteForceRenderChunk);
		chunksToRenderProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		chunksToRenderProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		ChunksToRender = std::make_shared<VulkanBuffer>(chunksToRenderProperties);
	}
	
	{
		VulkanBufferProperties resultCountProperties;
		resultCountProperties.Size = (uint32_t)sizeof(uint32_t);
		resultCountProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		resultCountProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		ResultCount = std::make_shared<VulkanBufferSet>(VulkanSwapchain::framesInFlight, resultCountProperties);
	}

	{
		VulkanBufferProperties computePassInfoProperties;
		computePassInfoProperties.Size = (uint32_t)sizeof(GPUComputePassInfo);
		computePassInfoProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		computePassInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_ComputePassBufferData = std::make_shared<VulkanBuffer>(computePassInfoProperties);
	}

	{
		std::shared_ptr<VulkanShader>& bruteForceConstruct = ShaderManager::createShader(CONSTRUCT_TERRAIN_CHUNKS_BRUTE_FORCE_COMPUTE);
		bruteForceConstruct->addShaderStage(ShaderStage::COMPUTE, CONSTRUCT_TERRAIN_CHUNKS_BRUTE_FORCE_COMPUTE);
		bruteForceConstruct->createDescriptorSetLayouts();

		m_ConstructTerrainChunksPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(CONSTRUCT_TERRAIN_CHUNKS_BRUTE_FORCE_COMPUTE));
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 0, 0, m_ComputePassBufferData);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 1, 0, m_FrustumBuffer);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 2, 0, infoBuffer);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(1, 0, 0, ChunksToRender);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(1, 1, 0, ResultCount);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(2, 0, 0, m_DrawIndirectCommandsSet);
		m_ConstructTerrainChunksPass.DescriptorSet->Create();
		m_ConstructTerrainChunksPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(CONSTRUCT_TERRAIN_CHUNKS_BRUTE_FORCE_COMPUTE));
	}
}
