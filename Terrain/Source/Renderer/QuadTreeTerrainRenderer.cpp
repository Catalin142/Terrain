#include "QuadTreeTerrainRenderer.h"

#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Terrain/VirtualMap/DynamicVirtualTerrainDeserializer.h"
#include "Terrain/VirtualMap/VirtualTerrainSerializer.h"

#define LOD_COMPUTE_SHADER "Terrain/LODMap_comp.glsl"

#define QUAD_TREE_TERRAIN_RENDER_SHADER_NAME "_TerrainQuadShader"
#define QUAD_TREE_TERRAIN_RENDER_VERTEX_SHADER_PATH "Terrain/QuadTree/Terrain_vert.glsl"
#define QUAD_TREE_TERRAIN_RENDER_FRAGMENT_SHADER_PATH "Terrain/Terrain_frag.glsl"

#define INDIRECT_COMMAND_SHADER "Terrain/QuadTree/CreateDrawCommand_comp.glsl"

QuadTreeTerrainRenderer::QuadTreeTerrainRenderer(const QuadTreeTerrainRendererSpecification& spec)
	: m_TargetFramebuffer(spec.TargetFramebuffer), m_Terrain(spec.Terrain)
{
	{
		VulkanBufferProperties terrainInfoProperties;
		terrainInfoProperties.Size = ((uint32_t)sizeof(TerrainInfo));
		terrainInfoProperties.Type = BufferType::UNIFORM_BUFFER;
		terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_TerrainInfo = std::make_shared<VulkanBuffer>(terrainInfoProperties);
	}
	{
		VulkanBufferProperties indirectProperties;
		indirectProperties.Size = sizeof(VkDrawIndexedIndirectCommand);
		indirectProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		indirectProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_DrawIndirectCommandsSet = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), indirectProperties);
	}

	m_VirtualMap = std::make_shared<TerrainVirtualMap>(spec.VirtualMapSpecification, m_Terrain);

	m_QuadTreeLOD = std::make_shared<QuadTreeLOD>(m_Terrain->getSpecification(), m_VirtualMap);

	createLODMapPipeline();
	createRenderPass();
	createPipeline();
	createIndirectCommandPass();
	createChunkIndexBuffer(1);
}

void QuadTreeTerrainRenderer::refreshVirtualMap(const Camera& camera)
{
	Instrumentor::Get().beginTimer(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES);

	glm::vec3 camPosition = camera.getPosition();
	m_VirtualMap->pushLoadTasks(glm::vec2(camPosition.x, camPosition.z));

	Instrumentor::Get().endTimer(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES);
}

void QuadTreeTerrainRenderer::updateVirtualMap()
{
	VkCommandBuffer vkCommandBuffer = CommandBuffer->getCurrentCommandBuffer();

	// Load nodes and update virtual map
	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_UPDATE_VIRTUAL_MAP);

		m_VirtualMap->updateMap(vkCommandBuffer);

		VulkanComputePipeline::imageMemoryBarrier(vkCommandBuffer, m_VirtualMap->getPhysicalTexture(), VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 1);

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_UPDATE_VIRTUAL_MAP);
	}

	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_UPDATE_STATUS_TEXTURE);
		m_VirtualMap->updateStatusTexture(vkCommandBuffer);
		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_UPDATE_STATUS_TEXTURE);
	}

	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_UPDATE_INDIRECTION_TEXTURE);
		m_VirtualMap->updateIndirectionTexture(vkCommandBuffer);
		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_UPDATE_INDIRECTION_TEXTURE);
	}

	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_GENERATE_QUAD_TREE);
		TerrainInfo terrainInfo = m_Terrain->getInfo();
		int32_t maxChunkSize = terrainInfo.ChunkSize << (terrainInfo.LODCount - 1);
		int32_t maxChunkCount = terrainInfo.TerrainSize / maxChunkSize;

		std::vector<TerrainChunk> quadTreeFirstPass;
		for (int32_t y = 0; y < maxChunkCount; y++)
			for (int32_t x = 0; x < maxChunkCount; x++)
				quadTreeFirstPass.push_back(TerrainChunk{ packOffset(x, y), terrainInfo.LODCount - 1 });

		m_QuadTreeLOD->Generate(vkCommandBuffer, quadTreeFirstPass);
		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_GENERATE_QUAD_TREE);
	}
	// Generate LodMap
	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP);

		VulkanRenderer::dispatchCompute(vkCommandBuffer, m_LODMapComputePass, { 8, 8, 1 });

		m_LODMapComputePass->Pipeline->imageMemoryBarrier(vkCommandBuffer, m_LODMap, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP);
	}

	// After computing the pass data, create the draw command
	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND);

		uint32_t idxBuffer = m_ChunkIndexBuffer.IndicesCount;
		VulkanRenderer::dispatchCompute(vkCommandBuffer, m_IndirectPass, { 1, 1, 1 },
			sizeof(uint32_t), &idxBuffer);

		VulkanComputePipeline::bufferMemoryBarrier(vkCommandBuffer, m_DrawIndirectCommandsSet->getBuffer(VulkanRenderer::getCurrentFrame()), VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND);
	}
}

void QuadTreeTerrainRenderer::Render(const Camera& camera)
{
	CommandBuffer->beginQuery(QuadTreeRendererMetrics::RENDER_TERRAIN);

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	TerrainInfo terrainInfo = m_Terrain->getInfo();
	m_TerrainInfo->setDataCPU(&terrainInfo, sizeof(TerrainInfo));

	VulkanRenderer::beginRenderPass(CommandBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(CommandBuffer, m_TerrainRenderPass);

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(CommandBuffer->getCurrentCommandBuffer(), m_TerrainRenderPass->Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	vkCmdBindIndexBuffer(CommandBuffer->getCurrentCommandBuffer(), m_ChunkIndexBuffer.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexedIndirect(CommandBuffer->getCurrentCommandBuffer(), m_DrawIndirectCommandsSet->getVkBuffer(VulkanRenderer::getCurrentFrame()), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

	VulkanRenderer::endRenderPass(CommandBuffer);

	CommandBuffer->endQuery(QuadTreeRendererMetrics::RENDER_TERRAIN);
}

void QuadTreeTerrainRenderer::setWireframe(bool wireframe)
{
	vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());
	if (m_InWireframe != wireframe)
	{
		m_InWireframe = wireframe;
		createPipeline();
	}
}

void QuadTreeTerrainRenderer::createChunkIndexBuffer(uint8_t lod)
{
	uint32_t vertCount = m_Terrain->getInfo().ChunkSize + 1;
	std::vector<uint32_t> indices = TerrainChunk::generateIndices(lod, vertCount);

	m_ChunkIndexBuffer.IndicesCount = (uint32_t)indices.size();

	VulkanBufferProperties indexBufferProperties;
	indexBufferProperties.Size = (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size());
	indexBufferProperties.Type = BufferType::INDEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	indexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	m_ChunkIndexBuffer.IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), indexBufferProperties);
}

void QuadTreeTerrainRenderer::createLODMapPipeline()
{
	{
		std::shared_ptr<VulkanShader>& lodCompute = ShaderManager::createShader(LOD_COMPUTE_SHADER);
		lodCompute->addShaderStage(ShaderStage::COMPUTE, LOD_COMPUTE_SHADER);
		lodCompute->createDescriptorSetLayouts();

		VulkanImageSpecification LODMapSpecification;
		LODMapSpecification.Width = uint32_t(m_Terrain->getInfo().TerrainSize) / m_Terrain->getInfo().ChunkSize;
		LODMapSpecification.Height = uint32_t(m_Terrain->getInfo().TerrainSize) / m_Terrain->getInfo().ChunkSize;
		LODMapSpecification.Format = VK_FORMAT_R8_UINT;
		LODMapSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		LODMapSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		m_LODMap = std::make_shared<VulkanImage>(LODMapSpecification);
		m_LODMap->Create();

		m_LODMapComputePass = std::make_shared<VulkanComputePass>();
		m_LODMapComputePass->DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(LOD_COMPUTE_SHADER));
		m_LODMapComputePass->DescriptorSet->bindInput(0, 0, 0, m_LODMap);
		m_LODMapComputePass->DescriptorSet->bindInput(0, 1, 0, m_QuadTreeLOD->chunksToRender);
		m_LODMapComputePass->DescriptorSet->bindInput(0, 2, 0, m_QuadTreeLOD->passMetadata);
		m_LODMapComputePass->DescriptorSet->Create();
		m_LODMapComputePass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(LOD_COMPUTE_SHADER));
	}
}

void QuadTreeTerrainRenderer::createRenderPass()
{
	m_TerrainRenderPass = std::make_shared<RenderPass>();
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader(QUAD_TREE_TERRAIN_RENDER_SHADER_NAME);
		mainShader->addShaderStage(ShaderStage::VERTEX, QUAD_TREE_TERRAIN_RENDER_VERTEX_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::FRAGMENT, QUAD_TREE_TERRAIN_RENDER_FRAGMENT_SHADER_PATH);
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(QUAD_TREE_TERRAIN_RENDER_SHADER_NAME));
		DescriptorSet->bindInput(0, 0, 0, m_QuadTreeLOD->chunksToRender);
		DescriptorSet->bindInput(0, 1, 0, m_TerrainInfo);
		DescriptorSet->bindInput(0, 2, 0, m_LODMap);
		DescriptorSet->bindInput(1, 0, 0, m_VirtualMap->getPhysicalTexture());
		for (uint32_t i = 0; i < MAX_LOD; i++)
		{
			DescriptorSet->bindInput(1, 1, i, m_VirtualMap->getIndirectionTexture(), (uint32_t)i);
		}
		DescriptorSet->bindInput(1, 3, 0, m_Terrain->getCompositionMap());
		DescriptorSet->bindInput(1, 4, 0, m_Terrain->getNormalMap());
		DescriptorSet->bindInput(2, 0, 0, m_Terrain->getTerrainTextures());
		DescriptorSet->bindInput(2, 1, 0, m_Terrain->getNormalTextures());

		DescriptorSet->Create();
		m_TerrainRenderPass->DescriptorSet = DescriptorSet;
	}
}

void QuadTreeTerrainRenderer::createPipeline()
{
	PipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader(QUAD_TREE_TERRAIN_RENDER_SHADER_NAME);
	spec.vertexBufferLayout = VulkanVertexBufferLayout{};
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass->Pipeline = m_TerrainPipeline;
}

void QuadTreeTerrainRenderer::createIndirectCommandPass()
{
	{
		VulkanBufferProperties indirectProperties;
		indirectProperties.Size = sizeof(VkDrawIndexedIndirectCommand);
		indirectProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		indirectProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_DrawIndirectCommandsSet = std::make_shared<VulkanBufferSet>(2, indirectProperties);
	}
	{
		std::shared_ptr<VulkanShader>& indirectCompute = ShaderManager::createShader(INDIRECT_COMMAND_SHADER);
		indirectCompute->addShaderStage(ShaderStage::COMPUTE, INDIRECT_COMMAND_SHADER);
		indirectCompute->createDescriptorSetLayouts();

		m_IndirectPass = std::make_shared<VulkanComputePass>();
		m_IndirectPass->DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(INDIRECT_COMMAND_SHADER));
		m_IndirectPass->DescriptorSet->bindInput(0, 0, 0, m_DrawIndirectCommandsSet);
		m_IndirectPass->DescriptorSet->bindInput(0, 1, 0, m_QuadTreeLOD->passMetadata);
		m_IndirectPass->DescriptorSet->Create();
		m_IndirectPass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(INDIRECT_COMMAND_SHADER),
			uint32_t(sizeof(uint32_t) * 4));
	}
}
