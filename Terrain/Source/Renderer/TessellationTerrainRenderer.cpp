#include "TessellationTerrainRenderer.h"

#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanUtils.h"

#include "Graphics/Vulkan/VulkanInitializers.h"

#include "Core/Instrumentor.h"
#include "Core/VulkanMemoryTracker.h"

#define TESSELLATION_TERRAIN_RENDER_SHADER_NAME "_TerrainTesselationShader"
#define TESSELLATION_TERRAIN_RENDER_VERTEX_SHADER_PATH "Terrain/ClipmapTessellation/Terrain_vert.glsl"
#define TESSELLATION_TERRAIN_RENDER_TESSELLATION_CONTROL_SHADER_PATH "Terrain/ClipmapTessellation/Terrain_tesc.glsl"
#define TESSELLATION_TERRAIN_RENDER_TESSELLATION_EVALUATION_SHADER_PATH "Terrain/ClipmapTessellation/Terrain_tese.glsl"
#define TESSELLATION_TERRAIN_RENDER_FRAGMENT_SHADER_PATH "Terrain/Terrain_frag.glsl"

#define VERTICAL_ERROR_COMPUTE_SHADER "Terrain/ClipmapTessellation/CreateVerticalErrorMap_comp.glsl"

TessellationTerrainRenderer::TessellationTerrainRenderer(const TessellationTerrainRendererSpecification& spec)
	: m_TargetFramebuffer(spec.TargetFramebuffer), m_Terrain(spec.Terrain), m_ControlPointSize(spec.ControlPointSize),
	m_ControlPointsPerRow(spec.ControlPointsPerRow)
{
	SimpleVulkanMemoryTracker::Get()->Flush(TessellationRendererMetrics::NAME);
	SimpleVulkanMemoryTracker::Get()->Track(TessellationRendererMetrics::NAME);
	
	{
		VulkanBufferProperties thresholdProperties;
		thresholdProperties.Size = ((uint32_t)sizeof(float)) * 6;
		thresholdProperties.Type = BufferType::STORAGE_BUFFER;
		thresholdProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_ThresholdBuffer = std::make_shared<VulkanBuffer>(thresholdProperties);
	}

	{
		VulkanBufferProperties tessellationSettingProperties;
		tessellationSettingProperties.Size = ((uint32_t)sizeof(int32_t)) * 2;
		tessellationSettingProperties.Type = BufferType::STORAGE_BUFFER;
		tessellationSettingProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_TessellationSettings = std::make_shared<VulkanBuffer>(tessellationSettingProperties);
	}

	{
		VulkanBufferProperties frustumBufferProperties;
		frustumBufferProperties.Size = ((uint32_t)sizeof(glm::vec4)) * 6;
		frustumBufferProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		frustumBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_FrustumBuffer = std::make_shared<VulkanBuffer>(frustumBufferProperties);
	}

	m_Clipmap = std::make_shared<TerrainClipmap>(spec.ClipmapSpecification, m_Terrain);
	m_TessellationLOD = std::make_shared<TessellationLOD>(m_Terrain->getSpecification(), m_Clipmap);
	m_TessellationLOD->createResources(m_Terrain, m_TessellationSettings);

	m_Clipmap->hardLoad(glm::vec2(spec.CameraStartingPosition.x, spec.CameraStartingPosition.z));

	uint32_t patchSize = m_ControlPointsPerRow * m_ControlPointSize;
	m_TessellationLOD->computeMargins(m_Clipmap->getLastValidCameraPosition(), patchSize);

	createVertexBuffer();

	createVerticalErrorComputePass();
	
	createRenderPass();
	createPipeline();

	SimpleVulkanMemoryTracker::Get()->Stop();
	TessellationRendererMetrics::MEMORY_USED = SimpleVulkanMemoryTracker::Get()->getAllocatedMemory(TessellationRendererMetrics::NAME);
}

void TessellationTerrainRenderer::refreshClipmaps()
{
	Instrumentor::Get().beginTimer(TessellationRendererMetrics::CPU_LOAD_NEEDED_NODES);

	glm::vec3 camPosition = SceneCamera.getPosition();
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

	uint32_t patchSize = m_ControlPointsPerRow * m_ControlPointSize;

	Instrumentor::Get().beginTimer(TessellationRendererMetrics::CPU_CREATE_CHUNK_BUFFER);
	if (chunksLoaded != 0)
	{
		TessellationRendererMetrics::CHUNKS_LOADED_LAST_UPDATE = chunksLoaded;
		m_TessellationLOD->computeMargins(m_Clipmap->getLastValidCameraPosition(), patchSize);
		m_VericalErrorMapGenerated = false;
	}
	Instrumentor::Get().endTimer(TessellationRendererMetrics::CPU_CREATE_CHUNK_BUFFER);

	CommandBuffer->beginQuery(TessellationRendererMetrics::GPU_CREATE_VERTICAL_ERROR_MAP);
	if (!m_VericalErrorMapGenerated)
	{
		VulkanRenderer::dispatchCompute(cmdBuffer, m_VerticalErrorPass, { 64, 64, m_Terrain->getInfo().LODCount});

		VulkanUtils::imageMemoryBarrier(cmdBuffer, m_VerticalErrorMap, { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT });

		m_VericalErrorMapGenerated = true;
	}
	CommandBuffer->endQuery(TessellationRendererMetrics::GPU_CREATE_VERTICAL_ERROR_MAP);

	CommandBuffer->beginQuery(TessellationRendererMetrics::GPU_GENERATE_AND_FRUSTUM_CULL);
	m_TessellationLOD->Generate(cmdBuffer, SceneCamera, patchSize);

	VulkanUtils::bufferMemoryBarrier(cmdBuffer, m_TessellationLOD->ChunksToRender, { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT });

	CommandBuffer->endQuery(TessellationRendererMetrics::GPU_GENERATE_AND_FRUSTUM_CULL);
}

void TessellationTerrainRenderer::Render(const Camera& camera)
{
	CommandBuffer->beginPipelineQuery();

	int32_t tessSettings[4] = {m_ControlPointSize, m_ControlPointsPerRow};
	m_TessellationSettings->setDataCPU(&tessSettings, 4 * sizeof(int32_t));

	CommandBuffer->beginQuery(TessellationRendererMetrics::RENDER_TERRAIN);

	m_ThresholdBuffer->setDataCPU(&Threshold, sizeof(float) * 6);

	std::array<glm::vec4, 6> frustumPlanes = SceneCamera.getFrustum();
	m_FrustumBuffer->setDataCPU(&frustumPlanes, 6 * sizeof(glm::vec4));

	VkCommandBuffer cmdBuffer = CommandBuffer->getCurrentCommandBuffer();
	VulkanRenderer::beginRenderPass(cmdBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(cmdBuffer, m_TerrainRenderPass, m_TessellationLOD->getMostRecentIndex());

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(cmdBuffer, m_TerrainRenderPass.Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdDrawIndirect(cmdBuffer, m_TessellationLOD->RenderCommand->getBuffer(), 0, 1, sizeof(VkDrawIndirectCommand));

	VulkanRenderer::endRenderPass(cmdBuffer);

	CommandBuffer->endQuery(TessellationRendererMetrics::RENDER_TERRAIN);

	CommandBuffer->endPipelineQuery();
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
		DescriptorSet->bindInput(0, 0, 0, m_TessellationSettings);
		DescriptorSet->bindInput(0, 1, 0, m_TessellationLOD->ChunksToRender);
		DescriptorSet->bindInput(1, 0, 0, m_VerticalErrorMap);
		DescriptorSet->bindInput(1, 1, 0, m_ThresholdBuffer);
		DescriptorSet->bindInput(1, 2, 0, m_TessellationLOD->LODMarginsBufferSet);
		DescriptorSet->bindInput(2, 0, 0, m_Clipmap->getMap());
		DescriptorSet->bindInput(2, 1, 0, m_Terrain->TerrainInfoBuffer);
		DescriptorSet->Create();
		m_TerrainRenderPass.DescriptorSet = DescriptorSet;
	}
}

void TessellationTerrainRenderer::createPipeline()
{
	RenderPipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.Shader = ShaderManager::getShader(TESSELLATION_TERRAIN_RENDER_SHADER_NAME);

	spec.vertexBufferLayout = VulkanVertexBufferLayout{ VertexType::INT_2 };
	spec.Topology = PrimitiveTopology::PATCHES;

	DepthStencilStateSpecification dsSpecification{};
	dsSpecification.depthTest = true;
	dsSpecification.depthWrite = true;
	dsSpecification.depthCompareFunction = DepthCompare::LESS;
	spec.depthStateSpecification = dsSpecification;

	ResterizationStateSpecification restarizationSpecification{};
	restarizationSpecification.Culling = true;
	restarizationSpecification.Wireframe = m_InWireframe;
	spec.resterizationSpecification = restarizationSpecification;

	spec.tessellationPatchControlPoints = 4;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), ShaderStage::TESSELLATION_EVALUATION });

	m_TerrainPipeline = std::make_shared<VulkanRenderPipeline>(spec);

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
			normalSpecification.LayerCount = m_Terrain->getInfo().LODCount;
			normalSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			m_VerticalErrorMap = std::make_shared<VulkanImage>(normalSpecification);
			m_VerticalErrorMap->Create();
		}

		m_VerticalErrorPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(VERTICAL_ERROR_COMPUTE_SHADER));
		m_VerticalErrorPass.DescriptorSet->bindInput(0, 0, 0, m_Clipmap->getMap());
		m_VerticalErrorPass.DescriptorSet->bindInput(0, 1, 0, m_VerticalErrorMap);
		m_VerticalErrorPass.DescriptorSet->Create();

		m_VerticalErrorPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(VERTICAL_ERROR_COMPUTE_SHADER));
	}
}

void TessellationTerrainRenderer::createVertexBuffer()
{
	std::vector<glm::ivec2> vertices;

	for (uint32_t patch = 0; patch < m_ControlPointsPerRow * m_ControlPointsPerRow; patch++)
	{
		vertices.push_back({ 0, 0 });
		vertices.push_back({ 0, 1 });
		vertices.push_back({ 1, 1 });
		vertices.push_back({ 1, 0 });
	}

	VulkanBufferProperties vertexBufferProperties;
	vertexBufferProperties.Size = (uint32_t)(sizeof(glm::ivec2) * (uint32_t)vertices.size());
	vertexBufferProperties.Type = BufferType::VERTEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	vertexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	m_VertexBuffer = std::make_shared<VulkanBuffer>(vertices.data(), vertexBufferProperties);
}
