#include "ClipmapTerrainRenderer.h"

#include "Terrain/VirtualMap/VirtualTerrainSerializer.h"

#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanRenderer.h"

#include "Core/Instrumentor.h"
#include "Core/VulkanMemoryTracker.h"

#define CLIPMAP_TERRAIN_RENDER_SHADER_NAME "_TerrainClipmapShader"
#define CLIPMAP_TERRAIN_RENDER_VERTEX_SHADER_PATH "Terrain/Clipmap/Terrain_vert.glsl"
#define CLIPMAP_TERRAIN_RENDER_FRAGMENT_SHADER_PATH "Terrain/Terrain_frag.glsl"

ClipmapTerrainRenderer::ClipmapTerrainRenderer(const ClipmapTerrainRendererSpecification& spec)
	: m_TargetFramebuffer(spec.TargetFramebuffer), m_Terrain(spec.Terrain)
{
	SimpleVulkanMemoryTracker::Get()->Flush(ClipmapRendererMetrics::NAME);
	SimpleVulkanMemoryTracker::Get()->Track(ClipmapRendererMetrics::NAME);

	{
		VulkanBufferProperties terrainInfoProperties;
		terrainInfoProperties.Size = ((uint32_t)sizeof(TerrainInfo));
		terrainInfoProperties.Type = BufferType::UNIFORM_BUFFER;
		terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_TerrainInfoBuffer = std::make_shared<VulkanBuffer>(terrainInfoProperties);
	}

	m_Clipmap = std::make_shared<TerrainClipmap>(spec.ClipmapSpecification, m_Terrain);
	m_ClipmapLOD = std::make_shared<ClipmapLOD>(m_Terrain->getSpecification(), m_Clipmap);

	m_Clipmap->hardLoad(glm::vec2(spec.CameraStartingPosition.x, spec.CameraStartingPosition.z));
	m_ChunksToRender = m_ClipmapLOD->Generate(m_Clipmap->getLastValidCameraPosition());

	createChunkIndexBuffer(1);
	createRenderPass();
	createPipeline();

	ClipmapRendererMetrics::MAX_VERTICES_RENDERED		= m_ChunksToRender * 129 * 129;
	ClipmapRendererMetrics::MAX_INDICES_RENDERED		= m_ChunksToRender * m_ChunkIndexBuffer.IndicesCount;
	ClipmapRendererMetrics::CHUNKS_LOADED_LAST_UPDATE	= m_ChunksToRender;

	SimpleVulkanMemoryTracker::Get()->Stop();
	ClipmapRendererMetrics::MEMORY_USED = SimpleVulkanMemoryTracker::Get()->getAllocatedMemory(ClipmapRendererMetrics::NAME);
}

void ClipmapTerrainRenderer::refreshClipmaps(const Camera& camera)
{
	Instrumentor::Get().beginTimer(ClipmapRendererMetrics::CPU_LOAD_NEEDED_NODES);

	glm::vec3 camPosition = camera.getPosition();
	glm::vec2 camPos = { camPosition.x, camPosition.z };
	m_Clipmap->pushLoadTasks(camPos);

	Instrumentor::Get().endTimer(ClipmapRendererMetrics::CPU_LOAD_NEEDED_NODES);
}

void ClipmapTerrainRenderer::updateClipmaps()
{
	CommandBuffer->beginQuery(ClipmapRendererMetrics::GPU_UPDATE_CLIPMAP);
	uint32_t chunksLoaded = m_Clipmap->updateClipmaps(CommandBuffer->getCurrentCommandBuffer());
	CommandBuffer->endQuery(ClipmapRendererMetrics::GPU_UPDATE_CLIPMAP);

	Instrumentor::Get().beginTimer(ClipmapRendererMetrics::CPU_CREATE_CHUNK_BUFFER);
	if (chunksLoaded != 0)
	{
		ClipmapRendererMetrics::CHUNKS_LOADED_LAST_UPDATE = chunksLoaded;

		m_ChunksToRender = m_ClipmapLOD->Generate(m_Clipmap->getLastValidCameraPosition());

		ClipmapRendererMetrics::MAX_VERTICES_RENDERED = m_ChunksToRender * 129 * 129;
		ClipmapRendererMetrics::MAX_INDICES_RENDERED = m_ChunksToRender * m_ChunkIndexBuffer.IndicesCount;
	}
	Instrumentor::Get().endTimer(ClipmapRendererMetrics::CPU_CREATE_CHUNK_BUFFER);
}

void ClipmapTerrainRenderer::Render(const Camera& camera)
{
	CommandBuffer->beginQuery(ClipmapRendererMetrics::RENDER_TERRAIN);

	TerrainInfo terrainInfo = m_Terrain->getInfo();
	m_TerrainInfoBuffer->setDataCPU(&terrainInfo, sizeof(TerrainInfo));

	VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();

	VulkanRenderer::beginRenderPass(commandBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(commandBuffer, m_TerrainRenderPass, m_ClipmapLOD->getMostRecentIndex());

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(commandBuffer, m_TerrainRenderPass.Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);
	
	vkCmdBindIndexBuffer(commandBuffer, m_ChunkIndexBuffer.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	
	VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer()};
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	
	vkCmdDrawIndexed(commandBuffer, m_ChunkIndexBuffer.IndicesCount, m_ChunksToRender, 0, 0, 0);

	VulkanRenderer::endRenderPass(commandBuffer);

	CommandBuffer->endQuery(ClipmapRendererMetrics::RENDER_TERRAIN);
}

void ClipmapTerrainRenderer::setWireframe(bool wireframe)
{
	if (m_InWireframe != wireframe)
	{
		vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());

		m_InWireframe = wireframe;
		createPipeline();
	}
}

void ClipmapTerrainRenderer::createRenderPass()
{
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader(CLIPMAP_TERRAIN_RENDER_SHADER_NAME);
		mainShader->addShaderStage(ShaderStage::VERTEX, CLIPMAP_TERRAIN_RENDER_VERTEX_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::FRAGMENT, CLIPMAP_TERRAIN_RENDER_FRAGMENT_SHADER_PATH);
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(CLIPMAP_TERRAIN_RENDER_SHADER_NAME));
		DescriptorSet->bindInput(0, 0, 0, m_ClipmapLOD->chunksToRender);
		DescriptorSet->bindInput(0, 1, 0, m_TerrainInfoBuffer);
		DescriptorSet->bindInput(0, 2, 0, m_ClipmapLOD->LODMarginsBufferSet);
		DescriptorSet->bindInput(1, 0, 0, m_Clipmap->getMap());
		DescriptorSet->bindInput(1, 3, 0, m_Terrain->getCompositionMap());
		DescriptorSet->bindInput(1, 4, 0, m_Terrain->getNormalMap());
		DescriptorSet->bindInput(2, 0, 0, m_Terrain->getTerrainTextures());
		DescriptorSet->bindInput(2, 1, 0, m_Terrain->getNormalTextures());

		DescriptorSet->Create();
		m_TerrainRenderPass.DescriptorSet = DescriptorSet;
	}
}

void ClipmapTerrainRenderer::createPipeline()
{
	PipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader(CLIPMAP_TERRAIN_RENDER_SHADER_NAME);
	spec.vertexBufferLayout = VulkanVertexBufferLayout{ VertexType::INT_2 };
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass.Pipeline = m_TerrainPipeline;
}

void ClipmapTerrainRenderer::createChunkIndexBuffer(uint8_t lod)
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
