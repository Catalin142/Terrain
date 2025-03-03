#include "TessellationTerrainRenderer.h"

#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanRenderer.h"

#include "Core/Instrumentor.h"
#include "Core/VulkanMemoryTracker.h"

#define TESSELLATION_TERRAIN_RENDER_SHADER_NAME "_TerrainTesselationShader"
#define TESSELLATION_TERRAIN_RENDER_VERTEX_SHADER_PATH "Terrain/ClipmapTessellation/Terrain_vert.glsl"
#define TESSELLATION_TERRAIN_RENDER_TESSELLATION_CONTROL_SHADER_PATH "Terrain/ClipmapTessellation/Terrain_tesc.glsl"
#define TESSELLATION_TERRAIN_RENDER_TESSELLATION_EVALUATION_SHADER_PATH "Terrain/ClipmapTessellation/Terrain_tese.glsl"
#define TESSELLATION_TERRAIN_RENDER_FRAGMENT_SHADER_PATH "Terrain/Terrain_frag.glsl"

#define VERTICAL_ERROR_COMPUTE_SHADER "Terrain/ClipmapTessellation/CreateVerticalErrorMap_comp.glsl"

TessellationTerrainRenderer::TessellationTerrainRenderer(const TessellationTerrainRendererSpecification& spec)
	: m_TargetFramebuffer(spec.TargetFramebuffer), m_Terrain(spec.Terrain), m_ControlPointSize(spec.ControlPointSize)
{
	SimpleVulkanMemoryTracker::Get()->Flush(TessellationRendererMetrics::NAME);
	SimpleVulkanMemoryTracker::Get()->Track(TessellationRendererMetrics::NAME);

	{
		VulkanBufferProperties terrainInfoProperties;
		terrainInfoProperties.Size = ((uint32_t)sizeof(TerrainInfo));
		terrainInfoProperties.Type = BufferType::UNIFORM_BUFFER;
		terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_TerrainInfoBuffer = std::make_shared<VulkanBuffer>(terrainInfoProperties);
	}
	{
		VulkanBufferProperties terrainInfoProperties;
		terrainInfoProperties.Size = ((uint32_t)sizeof(glm::vec4));
		terrainInfoProperties.Type = BufferType::STORAGE_BUFFER;
		terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_ThresholdBuffer = std::make_shared<VulkanBuffer>(terrainInfoProperties);
	}

	m_Clipmap = std::make_shared<TerrainClipmap>(spec.ClipmapSpecification, m_Terrain);
	m_TessellationLOD = std::make_shared<TessellationLOD>(m_Terrain->getSpecification(), m_Clipmap);

	m_Clipmap->hardLoad(glm::vec2(spec.CameraStartingPosition.x, spec.CameraStartingPosition.z));
	m_ChunksToRender = m_TessellationLOD->Generate(m_Clipmap->getLastValidCameraPosition());

	createVertexBuffer();

	createVerticalErrorComputePass();
	
	createRenderPass();
	createPipeline();

	SimpleVulkanMemoryTracker::Get()->Stop();
	TessellationRendererMetrics::MEMORY_USED = SimpleVulkanMemoryTracker::Get()->getAllocatedMemory(TessellationRendererMetrics::NAME);
}

void TessellationTerrainRenderer::refreshClipmaps(const Camera& camera)
{
	Instrumentor::Get().beginTimer(TessellationRendererMetrics::CPU_LOAD_NEEDED_NODES);

	glm::vec3 camPosition = camera.getPosition();
	glm::vec2 camPos = { camPosition.x, camPosition.z };
	m_Clipmap->pushLoadTasks(camPos);

	Instrumentor::Get().endTimer(TessellationRendererMetrics::CPU_LOAD_NEEDED_NODES);
}

void TessellationTerrainRenderer::updateClipmaps()
{
	VkCommandBuffer cmdBuffer = CommandBuffer->getCurrentCommandBuffer();

	CommandBuffer->beginQuery(TessellationRendererMetrics::GPU_UPDATE_CLIPMAP);
	uint32_t chunksLoaded = m_Clipmap->updateClipmaps(cmdBuffer);
	CommandBuffer->endQuery(TessellationRendererMetrics::GPU_UPDATE_CLIPMAP);

	Instrumentor::Get().beginTimer(TessellationRendererMetrics::CPU_CREATE_CHUNK_BUFFER);
	if (chunksLoaded != 0)
	{
		TessellationRendererMetrics::CHUNKS_LOADED_LAST_UPDATE = chunksLoaded;
		m_ChunksToRender = m_TessellationLOD->Generate(m_Clipmap->getLastValidCameraPosition());
		m_VericalErrorMapGenerated = false;
	}
	Instrumentor::Get().endTimer(TessellationRendererMetrics::CPU_CREATE_CHUNK_BUFFER);

	CommandBuffer->beginQuery(TessellationRendererMetrics::GPU_CREATE_VERTICAL_ERROR_MAP);
	if (!m_VericalErrorMapGenerated)
	{
		VulkanRenderer::dispatchCompute(cmdBuffer, m_VerticalErrorPass, { 64, 64, 3 });

		VulkanComputePipeline::imageMemoryBarrier(cmdBuffer, m_VerticalErrorMap, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, 1, 3);

		m_VericalErrorMapGenerated = true;
	}
	CommandBuffer->endQuery(TessellationRendererMetrics::GPU_CREATE_VERTICAL_ERROR_MAP);
}

void TessellationTerrainRenderer::Render(const Camera& camera)
{
	CommandBuffer->beginQuery(TessellationRendererMetrics::RENDER_TERRAIN);

	m_ThresholdBuffer->setDataCPU(&Threshold, sizeof(glm::vec4));

	TerrainInfo terrainInfo = m_Terrain->getInfo();
	m_TerrainInfoBuffer->setDataCPU(&terrainInfo, sizeof(TerrainInfo));

	VkCommandBuffer cmdBuffer = CommandBuffer->getCurrentCommandBuffer();
	VulkanRenderer::beginRenderPass(cmdBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(cmdBuffer, m_TerrainRenderPass, m_TessellationLOD->getMostRecentIndex());

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(cmdBuffer, m_TerrainRenderPass.Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdDraw(cmdBuffer, 4, m_ChunksToRender * 64, 0, 0);

	VulkanRenderer::endRenderPass(cmdBuffer);

	CommandBuffer->endQuery(TessellationRendererMetrics::RENDER_TERRAIN);
}

void TessellationTerrainRenderer::setWireframe(bool wireframe)
{
	if (m_InWireframe != wireframe)
	{
		vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());

		m_InWireframe = wireframe;
		createPipeline();
	}
}

void TessellationTerrainRenderer::createRenderPass()
{
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader(TESSELLATION_TERRAIN_RENDER_SHADER_NAME);
		mainShader->addShaderStage(ShaderStage::VERTEX, TESSELLATION_TERRAIN_RENDER_VERTEX_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::TESSELLATION_CONTROL, TESSELLATION_TERRAIN_RENDER_TESSELLATION_CONTROL_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::TESSELLATION_EVALUATION, TESSELLATION_TERRAIN_RENDER_TESSELLATION_EVALUATION_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::FRAGMENT, TESSELLATION_TERRAIN_RENDER_FRAGMENT_SHADER_PATH);
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(TESSELLATION_TERRAIN_RENDER_SHADER_NAME));
		DescriptorSet->bindInput(0, 0, 0, m_TessellationLOD->chunksToRender);
		DescriptorSet->bindInput(1, 0, 0, m_VerticalErrorMap);
		DescriptorSet->bindInput(1, 1, 0, m_ThresholdBuffer);
		DescriptorSet->bindInput(1, 2, 0, m_TessellationLOD->LODMarginsBufferSet);
		DescriptorSet->bindInput(2, 0, 0, m_Clipmap->getMap());
		DescriptorSet->bindInput(2, 1, 0, m_TerrainInfoBuffer);
		DescriptorSet->Create();
		m_TerrainRenderPass.DescriptorSet = DescriptorSet;
	}
}

void TessellationTerrainRenderer::createPipeline()
{
	PipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader(TESSELLATION_TERRAIN_RENDER_SHADER_NAME);
	spec.vertexBufferLayout = VulkanVertexBufferLayout{ VertexType::INT_2 };
	spec.depthCompareFunction = DepthCompare::LESS;
	spec.Topology = PrimitiveTopology::PATCHES;
	spec.tessellationControlPoints = 4;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass.Pipeline = m_TerrainPipeline;
}

void TessellationTerrainRenderer::createVerticalErrorComputePass()
{
	m_VerticalErrorMapSize = m_Clipmap->getSpecification().ClipmapSize / m_ControlPointSize;
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader(VERTICAL_ERROR_COMPUTE_SHADER);
		mainShader->addShaderStage(ShaderStage::COMPUTE, VERTICAL_ERROR_COMPUTE_SHADER);
		mainShader->createDescriptorSetLayouts();
		{
			VulkanImageSpecification normalSpecification;
			normalSpecification.Width = m_VerticalErrorMapSize;
			normalSpecification.Height = m_VerticalErrorMapSize;
			normalSpecification.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
			normalSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			normalSpecification.LayerCount = 3;
			normalSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			m_VerticalErrorMap = std::make_shared<VulkanImage>(normalSpecification);
			m_VerticalErrorMap->Create();
		}

		m_VerticalErrorPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(VERTICAL_ERROR_COMPUTE_SHADER));
		m_VerticalErrorPass.DescriptorSet->bindInput(0, 0, 0, m_Clipmap->getMap());
		m_VerticalErrorPass.DescriptorSet->bindInput(0, 1, 0, m_VerticalErrorMap);
		m_VerticalErrorPass.DescriptorSet->bindInput(1, 0, 0, m_TessellationLOD->LODMarginsBufferSet);
		m_VerticalErrorPass.DescriptorSet->Create();

		m_VerticalErrorPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(VERTICAL_ERROR_COMPUTE_SHADER));

	}
}

void TessellationTerrainRenderer::createVertexBuffer()
{
	std::vector<glm::ivec2> vertices;

	vertices.push_back({ 0, 0 });
	vertices.push_back({ 0, 1 });
	vertices.push_back({ 1, 1 });
	vertices.push_back({ 1, 0 });

	VulkanBufferProperties vertexBufferProperties;
	vertexBufferProperties.Size = (uint32_t)(sizeof(glm::ivec2) * (uint32_t)vertices.size());
	vertexBufferProperties.Type = BufferType::VERTEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	vertexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	m_VertexBuffer = std::make_shared<VulkanBuffer>(vertices.data(), vertexBufferProperties);
}
