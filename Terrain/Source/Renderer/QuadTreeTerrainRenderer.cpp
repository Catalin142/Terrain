#include "QuadTreeTerrainRenderer.h"

#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Terrain/VirtualMap/DynamicVirtualTerrainDeserializer.h"
#include "Terrain/VirtualMap/VirtualTerrainSerializer.h"

#include "Core/VulkanMemoryTracker.h"

#define LOD_COMPUTE_SHADER "Terrain/QuadTree/LODMap_comp.glsl"

#define QUAD_TREE_TERRAIN_RENDER_SHADER_NAME "_TerrainQuadShader"
#define QUAD_TREE_TERRAIN_RENDER_VERTEX_SHADER_PATH "Terrain/QuadTree/Terrain_vert.glsl"
#define QUAD_TREE_TERRAIN_RENDER_FRAGMENT_SHADER_PATH "Terrain/Terrain_frag.glsl"

#define INDIRECT_COMMAND_SHADER "Terrain/QuadTree/CreateDrawCommand_comp.glsl"

#define NEIGHBOURS_COMPUTE_SHADER "Terrain/QuadTree/SetNeighbours_comp.glsl"

QuadTreeTerrainRenderer::QuadTreeTerrainRenderer(const QuadTreeTerrainRendererSpecification& spec)
	: m_TargetFramebuffer(spec.TargetFramebuffer), m_Terrain(spec.Terrain)
{
	SimpleVulkanMemoryTracker::Get()->Flush(QuadTreeRendererMetrics::NAME);
	SimpleVulkanMemoryTracker::Get()->Track(QuadTreeRendererMetrics::NAME);

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
	createNeighboursCommandPass();
	createRenderPass();
	createPipeline();
	createIndirectCommandPass();
	createChunkIndexBuffer(1);

	SimpleVulkanMemoryTracker::Get()->Stop();
	QuadTreeRendererMetrics::MEMORY_USED = SimpleVulkanMemoryTracker::Get()->getAllocatedMemory(QuadTreeRendererMetrics::NAME);
}

void QuadTreeTerrainRenderer::refreshVirtualMap(const Camera& camera)
{
	Instrumentor::Get().beginTimer(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES);

	glm::vec3 camPosition = camera.getPosition();

	uint32_t requestedChunks = m_VirtualMap->pushLoadTasks(glm::vec2(camPosition.x, camPosition.z));
	if (requestedChunks != 0)
	{
		QuadTreeRendererMetrics::MAX_VERTICES_RENDERED = requestedChunks * 129 * 129;
		QuadTreeRendererMetrics::MAX_INDICES_RENDERED = requestedChunks * m_ChunkIndexBuffer.IndicesCount;
	}
	Instrumentor::Get().endTimer(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES);
}

void QuadTreeTerrainRenderer::updateVirtualMap()
{
	VkCommandBuffer vkCommandBuffer = CommandBuffer->getCurrentCommandBuffer();
	uint32_t loadedChunks = 0;
	bool updated = false;

	// Load nodes and update virtual map
	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_UPDATE_VIRTUAL_MAP);

		loadedChunks = m_VirtualMap->updateMap(vkCommandBuffer);
		updated = m_VirtualMap->Updated();

		QuadTreeRendererMetrics::CHUNKS_LOADED_LAST_FRAME = loadedChunks;
		if (updated)
		{
			QuadTreeRendererMetrics::CHUNKS_LOADED_LAST_UPDATE = loadedChunks;

			m_BufferUsed = m_AvailableBuffer;
			m_AvailableBuffer++;
			m_AvailableBuffer %= VulkanRenderer::getFramesInFlight();

			VulkanComputePipeline::imageMemoryBarrier(vkCommandBuffer, m_VirtualMap->getPhysicalTexture(), VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 1);
		}

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

		if (updated)
		{
			TerrainInfo terrainInfo = m_Terrain->getInfo();
			int32_t maxChunkSize = terrainInfo.ChunkSize << (terrainInfo.LODCount - 1);
			int32_t maxChunkCount = terrainInfo.TerrainSize / maxChunkSize;

			std::vector<TerrainChunk> quadTreeFirstPass;
			for (int32_t y = 0; y < maxChunkCount; y++)
				for (int32_t x = 0; x < maxChunkCount; x++)
					quadTreeFirstPass.push_back(TerrainChunk{ packOffset(x, y), terrainInfo.LODCount - 1 });

			m_QuadTreeLOD->Generate(vkCommandBuffer, quadTreeFirstPass, m_BufferUsed);
		}

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_GENERATE_QUAD_TREE);
	}
	// Generate LodMap
	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP);

		if (updated)
		{
			TerrainInfo terrainInfo = m_Terrain->getInfo();
			uint32_t slots = uint32_t(terrainInfo.TerrainSize) / terrainInfo.ChunkSize;

			VulkanRenderer::dispatchCompute(vkCommandBuffer, m_LODMapComputePass, m_BufferUsed, { slots / 8, slots / 8, 1 });

			m_LODMapComputePass.Pipeline->imageMemoryBarrier(vkCommandBuffer, m_LODMap, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP);
	}

	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_SET_NEIGHTBOURS);

		if (updated)
		{
			VulkanRenderer::dispatchCompute(vkCommandBuffer, m_NeighboursComputePass, m_BufferUsed, { 2, 1, 1 });

			VulkanComputePipeline::bufferMemoryBarrier(vkCommandBuffer, m_QuadTreeLOD->ChunksToRender, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
		}

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_SET_NEIGHTBOURS);
	}

	// After computing the pass data, create the draw command
	{
		CommandBuffer->beginQuery(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND);

		if (updated)
		{
			uint32_t idxBuffer = m_ChunkIndexBuffer.IndicesCount;
			VulkanRenderer::dispatchCompute(vkCommandBuffer, m_IndirectComputePass, m_BufferUsed, { 1, 1, 1 }, sizeof(uint32_t), &idxBuffer);

			VulkanComputePipeline::bufferMemoryBarrier(vkCommandBuffer, m_DrawIndirectCommandsSet->getBuffer(m_BufferUsed), VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}

		CommandBuffer->endQuery(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND);
	}
}

void QuadTreeTerrainRenderer::Render(const Camera& camera)
{
	CommandBuffer->beginQuery(QuadTreeRendererMetrics::RENDER_TERRAIN);

	VkCommandBuffer cmdBuffer = CommandBuffer->getCurrentCommandBuffer();

	VulkanRenderer::beginRenderPass(cmdBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(cmdBuffer, m_TerrainRenderPass);

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(cmdBuffer, m_TerrainRenderPass.Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdBindIndexBuffer(cmdBuffer, m_ChunkIndexBuffer.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	
	vkCmdDrawIndexedIndirect(cmdBuffer, m_DrawIndirectCommandsSet->getVkBuffer(m_BufferUsed), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

	VulkanRenderer::endRenderPass(cmdBuffer);

	CommandBuffer->endQuery(QuadTreeRendererMetrics::RENDER_TERRAIN);
}

void QuadTreeTerrainRenderer::setWireframe(bool wireframe)
{
	if (m_InWireframe != wireframe)
	{
		vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());

		m_InWireframe = wireframe;
		createPipeline();
	}
}

void QuadTreeTerrainRenderer::createChunkIndexBuffer(uint8_t lod)
{
	uint32_t vertCount = m_Terrain->getInfo().ChunkSize + 1;
	std::vector<uint32_t> indices;
	std::vector<glm::ivec2> vertices;

	TerrainChunk::generateIndicesAndVertices(vertCount, indices, vertices);

	m_ChunkIndexBuffer.IndicesCount = (uint32_t)indices.size();

	VulkanBufferProperties indexBufferProperties;
	indexBufferProperties.Size = (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size());
	indexBufferProperties.Type = BufferType::INDEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	indexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	VulkanBufferProperties vertexBufferProperties;
	vertexBufferProperties.Size = (uint32_t)(sizeof(glm::ivec2) * (uint32_t)vertices.size());
	vertexBufferProperties.Type = BufferType::VERTEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	vertexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	m_ChunkIndexBuffer.IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), indexBufferProperties);
	m_VertexBuffer = std::make_shared<VulkanBuffer>(vertices.data(), vertexBufferProperties);
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

		m_LODMapComputePass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(LOD_COMPUTE_SHADER));
		m_LODMapComputePass.DescriptorSet->bindInput(0, 0, 0, m_LODMap);
		m_LODMapComputePass.DescriptorSet->bindInput(0, 1, 0, m_QuadTreeLOD->ChunksToRender);
		m_LODMapComputePass.DescriptorSet->bindInput(0, 2, 0, m_QuadTreeLOD->PassMetadata);
		m_LODMapComputePass.DescriptorSet->Create();
		m_LODMapComputePass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(LOD_COMPUTE_SHADER));
	}
}

void QuadTreeTerrainRenderer::createRenderPass()
{
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader(QUAD_TREE_TERRAIN_RENDER_SHADER_NAME);
		mainShader->addShaderStage(ShaderStage::VERTEX, QUAD_TREE_TERRAIN_RENDER_VERTEX_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::FRAGMENT, QUAD_TREE_TERRAIN_RENDER_FRAGMENT_SHADER_PATH);
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(QUAD_TREE_TERRAIN_RENDER_SHADER_NAME));
		DescriptorSet->bindInput(0, 0, 0, m_QuadTreeLOD->ChunksToRender);
		DescriptorSet->bindInput(0, 1, 0, m_Terrain->TerrainInfoBuffer);
		DescriptorSet->bindInput(0, 2, 0, m_LODMap);
		DescriptorSet->bindInput(1, 0, 0, m_VirtualMap->getPhysicalTexture());
		for (uint32_t i = 0; i < MAX_LOD; i++)
		{
			DescriptorSet->bindInput(1, 1, i, m_VirtualMap->getIndirectionTexture(), (uint32_t)i);
		}

		DescriptorSet->Create();
		m_TerrainRenderPass.DescriptorSet = DescriptorSet;
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
	spec.vertexBufferLayout = VulkanVertexBufferLayout{ VertexType::INT_2 };
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass.Pipeline = m_TerrainPipeline;
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

		m_IndirectComputePass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(INDIRECT_COMMAND_SHADER));
		m_IndirectComputePass.DescriptorSet->bindInput(0, 0, 0, m_DrawIndirectCommandsSet);
		m_IndirectComputePass.DescriptorSet->bindInput(0, 1, 0, m_QuadTreeLOD->PassMetadata);
		m_IndirectComputePass.DescriptorSet->Create();
		m_IndirectComputePass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(INDIRECT_COMMAND_SHADER),
			uint32_t(sizeof(uint32_t) * 4));
	}
}

void QuadTreeTerrainRenderer::createNeighboursCommandPass()
{
	{
		std::shared_ptr<VulkanShader>& neightboursCompute = ShaderManager::createShader(NEIGHBOURS_COMPUTE_SHADER);
		neightboursCompute->addShaderStage(ShaderStage::COMPUTE, NEIGHBOURS_COMPUTE_SHADER);
		neightboursCompute->createDescriptorSetLayouts();

		m_NeighboursComputePass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(NEIGHBOURS_COMPUTE_SHADER));
		m_NeighboursComputePass.DescriptorSet->bindInput(0, 0, 0, m_LODMap);
		m_NeighboursComputePass.DescriptorSet->bindInput(1, 0, 0, m_QuadTreeLOD->ChunksToRender);
		m_NeighboursComputePass.DescriptorSet->bindInput(1, 1, 0, m_Terrain->TerrainInfoBuffer);
		m_NeighboursComputePass.DescriptorSet->bindInput(1, 2, 0, m_QuadTreeLOD->PassMetadata);
		m_NeighboursComputePass.DescriptorSet->Create();
		m_NeighboursComputePass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(NEIGHBOURS_COMPUTE_SHADER));
	}
}
