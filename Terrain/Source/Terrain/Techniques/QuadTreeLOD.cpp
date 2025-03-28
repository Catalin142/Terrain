#include "QuadTreeLOD.h"

#include "Graphics/Vulkan/VulkanRenderer.h"

#include <cmath>

#include "Graphics/Vulkan/VulkanDevice.h"

#define CONSTRUCT_QUAD_COMPUTE "Terrain/QuadTree/ConstructQuadPass_comp.glsl"
#define QUAD_TREE_FRUSTUM_CULLING_COMPUTE "Terrain/QuadTree/FrustumCulling_comp.glsl"

QuadTreeLOD::QuadTreeLOD(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<TerrainVirtualMap>& virtualMap) 
	: m_TerrainSpecification(terrain->getSpecification())
{
	createQuadTreePass(virtualMap);
	createFrustumCullingPass(terrain->TerrainInfoBuffer);
}

void QuadTreeLOD::Generate(VkCommandBuffer commandBuffer, std::vector<TerrainChunk> firstPass, uint32_t bufferIndex)
{
	QuadTreePassMetadata metadata;
	metadata.ResultArrayIndex = 0;
	metadata.TMPArray1Index = 0;
	metadata.DataLoaded = firstPass.size();

	m_TempA->getBuffer(bufferIndex)->setDataCPU(firstPass.data(), firstPass.size() * sizeof(TerrainChunk));
	uint32_t sizefp = firstPass.size();

	PassMetadata->getBuffer(bufferIndex)->setDataCPU(&metadata, sizeof(QuadTreePassMetadata));

	VulkanComputePass computePass;
	computePass.Pipeline = m_ConstructQuadTreePipeline;

	for (uint32_t lod = 0; lod < m_TerrainSpecification.Info.LODCount; lod++)
	{
		computePass.DescriptorSet = m_DescriptorSets[lod % 2];
		VulkanRenderer::dispatchCompute(commandBuffer, computePass, bufferIndex, { 1, 1, 1 }, sizeof(uint32_t), &sizefp);

		VkMemoryBarrier barrier1 = {};
		barrier1.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			1, &barrier1,
			0, nullptr, 0, nullptr
		);

		uint32_t indexSSBO = 0;
		vkCmdUpdateBuffer(commandBuffer, PassMetadata->getVkBuffer(bufferIndex), 0, sizeof(uint32_t), &indexSSBO);

		VkMemoryBarrier barrier2 = {};
		barrier2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier2.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			1, &barrier2,
			0, nullptr, 0, nullptr
		);
	}

	VulkanComputePipeline::bufferMemoryBarrier(commandBuffer, PassMetadata->getBuffer(bufferIndex), VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	VulkanComputePipeline::bufferMemoryBarrier(commandBuffer, AllChunks, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void QuadTreeLOD::FrustumCull(VkCommandBuffer commandBuffer, Camera cam, uint32_t bufferIndex)
{
	std::array<glm::vec4, 6> frustumPlanes = cam.getFrustum();
	m_FrustumBuffer->setDataCPU(&frustumPlanes, 6 * sizeof(glm::vec4));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_FrustumCullingPass.Pipeline->getVkPipeline());

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_FrustumCullingPass.Pipeline->getVkPipelineLayout(),
		0, m_FrustumCullingPass.DescriptorSet->getNumberOfSets(),
		m_FrustumCullingPass.DescriptorSet->getDescriptorSet(bufferIndex).data(), 0, nullptr);

	vkCmdDispatchIndirect(commandBuffer, m_DispatchCommand->getBuffer(), 0);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getComputeQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
		assert(false);

	VulkanComputePipeline::bufferMemoryBarrier(commandBuffer, ChunksToRender, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
}

void QuadTreeLOD::createQuadTreePass(const std::shared_ptr<TerrainVirtualMap>& virtualMap)
{
	{
		VulkanBufferProperties tempAProperties;
		tempAProperties.Size = 1024 * (uint32_t)sizeof(TerrainChunk);
		tempAProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		tempAProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_TempA = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), tempAProperties);
	}
	{
		VulkanBufferProperties tempBProperties;
		tempBProperties.Size = 1024 * (uint32_t)sizeof(TerrainChunk);
		tempBProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_SRC_BUFFER;
		tempBProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_TempB = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), tempBProperties);
	}
	{
		VulkanBufferProperties resultProperties;
		resultProperties.Size = 1024 * (uint32_t)sizeof(GPUQuadTreeTerrainChunk);
		resultProperties.Type = BufferType::STORAGE_BUFFER;
		resultProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		AllChunks = std::make_shared<VulkanBuffer>(resultProperties);
	}
	{
		VulkanBufferProperties passMetadataProperties;
		passMetadataProperties.Size = sizeof(QuadTreePassMetadata);
		passMetadataProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		passMetadataProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		PassMetadata = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), passMetadataProperties);
	}
	{
		VulkanBufferProperties dispatchProperties;
		dispatchProperties.Size = sizeof(VkDispatchIndirectCommand);
		dispatchProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		dispatchProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_DispatchCommand = std::make_shared<VulkanBuffer>(dispatchProperties);
	}
	// create shader
	{
		std::shared_ptr<VulkanShader>& quadCompute = ShaderManager::createShader(CONSTRUCT_QUAD_COMPUTE);
		quadCompute->addShaderStage(ShaderStage::COMPUTE, CONSTRUCT_QUAD_COMPUTE);
		quadCompute->createDescriptorSetLayouts();
	}

	// create pass
	{
		m_DescriptorSets.resize(2);
		m_DescriptorSets[0] = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(CONSTRUCT_QUAD_COMPUTE));
		for (uint32_t i = 0; i < MAX_LOD; i++)
		{
			m_DescriptorSets[0]->bindInput(0, 0, i, virtualMap->getLoadStatusTexture(), (uint32_t)i);
			m_DescriptorSets[0]->bindInput(0, 1, i, virtualMap->getIndirectionTexture(), (uint32_t)i);
		}
		m_DescriptorSets[0]->bindInput(1, 0, 0, m_TempA);
		m_DescriptorSets[0]->bindInput(1, 1, 0, m_TempB);
		m_DescriptorSets[0]->bindInput(1, 2, 0, AllChunks);
		m_DescriptorSets[0]->bindInput(2, 0, 0, PassMetadata);
		m_DescriptorSets[0]->bindInput(3, 0, 0, m_DispatchCommand);
		m_DescriptorSets[0]->Create();

		m_DescriptorSets[1] = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(CONSTRUCT_QUAD_COMPUTE));
		for (uint32_t i = 0; i < MAX_LOD; i++)
		{
			m_DescriptorSets[1]->bindInput(0, 0, i, virtualMap->getLoadStatusTexture(), (uint32_t)i);
			m_DescriptorSets[1]->bindInput(0, 1, i, virtualMap->getIndirectionTexture(), (uint32_t)i);
		}
		m_DescriptorSets[1]->bindInput(1, 0, 0, m_TempB);
		m_DescriptorSets[1]->bindInput(1, 1, 0, m_TempA);
		m_DescriptorSets[1]->bindInput(1, 2, 0, AllChunks);
		m_DescriptorSets[1]->bindInput(2, 0, 0, PassMetadata);
		m_DescriptorSets[1]->bindInput(3, 0, 0, m_DispatchCommand);
		m_DescriptorSets[1]->Create();

		m_ConstructQuadTreePipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(CONSTRUCT_QUAD_COMPUTE),
			uint32_t(sizeof(uint32_t) * 4));
	}
}

void QuadTreeLOD::createFrustumCullingPass(const std::shared_ptr<VulkanBuffer>& infoBuffer)
{
	{
		VulkanBufferProperties resultProperties;
		resultProperties.Size = 1024 * (uint32_t)sizeof(GPUQuadTreeTerrainChunk);
		resultProperties.Type = BufferType::STORAGE_BUFFER;
		resultProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		ChunksToRender = std::make_shared<VulkanBuffer>(resultProperties);
	}
	{
		VulkanBufferProperties frustumBufferProperties;
		frustumBufferProperties.Size = ((uint32_t)sizeof(glm::vec4)) * 6;
		frustumBufferProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		frustumBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_FrustumBuffer = std::make_shared<VulkanBuffer>(frustumBufferProperties);
	}
	{
		VulkanBufferProperties renderProperties;
		renderProperties.Size = sizeof(VkDrawIndexedIndirectCommand);
		renderProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		renderProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_RenderCommand = std::make_shared<VulkanBuffer>(renderProperties);
	}

	{
		std::shared_ptr<VulkanShader>& frustumCullShader = ShaderManager::createShader(QUAD_TREE_FRUSTUM_CULLING_COMPUTE);
		frustumCullShader->addShaderStage(ShaderStage::COMPUTE, QUAD_TREE_FRUSTUM_CULLING_COMPUTE);
		frustumCullShader->createDescriptorSetLayouts();

		m_FrustumCullingPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(QUAD_TREE_FRUSTUM_CULLING_COMPUTE));
		m_FrustumCullingPass.DescriptorSet->bindInput(0, 0, 0, m_RenderCommand);
		m_FrustumCullingPass.DescriptorSet->bindInput(0, 1, 0, PassMetadata);
		m_FrustumCullingPass.DescriptorSet->bindInput(0, 2, 0, infoBuffer);
		m_FrustumCullingPass.DescriptorSet->bindInput(1, 0, 0, AllChunks);
		m_FrustumCullingPass.DescriptorSet->bindInput(1, 1, 0, ChunksToRender);
		m_FrustumCullingPass.DescriptorSet->bindInput(2, 0, 0, m_FrustumBuffer);
		m_FrustumCullingPass.DescriptorSet->Create();

		m_FrustumCullingPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(QUAD_TREE_FRUSTUM_CULLING_COMPUTE));
	}
}

