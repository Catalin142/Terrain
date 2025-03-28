#include "BruteForceTerrainRenderer.h"

#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanRenderer.h"

#include "Core/VulkanMemoryTracker.h"

#define BRUTE_FORCE_TERRAIN_RENDER_SHADER_NAME "_TerrainBruteForceShader"
#define BRUTE_FORCE_TERRAIN_RENDER_VERTEX_SHADER_PATH "Terrain/BruteForce/Terrain_vert.glsl"
#define BRUTE_FORCE_TERRAIN_RENDER_TESSELLATION_CONTROL_SHADER_PATH "Terrain/BruteForce/Terrain_tesc.glsl"
#define BRUTE_FORCE_TERRAIN_RENDER_TESSELLATION_EVALUATION_SHADER_PATH "Terrain/BruteForce/Terrain_tese.glsl"
#define BRUTE_FORCE_TERRAIN_RENDER_FRAGMENT_SHADER_PATH "Terrain/Terrain_frag.glsl"

BruteForceTerrainRenderer::BruteForceTerrainRenderer(const BruteForceTerrainRendererSpecification& specification) : 
	m_TargetFramebuffer(specification.TargetFramebuffer), m_Terrain(specification.Terrain)
{
	SimpleVulkanMemoryTracker::Get()->Flush(BruteForceRendererMetrics::NAME);
	SimpleVulkanMemoryTracker::Get()->Track(BruteForceRendererMetrics::NAME);

	m_TerrainMap = std::make_shared<TerrainMap>(specification.HeightMapFilepath);
	m_BruteForceLOD = std::make_shared<BruteForceLOD>(m_Terrain);

	createVertexBuffer();

	createRenderPass();
	createPipeline();

	SimpleVulkanMemoryTracker::Get()->Stop();
	BruteForceRendererMetrics::MEMORY_USED = SimpleVulkanMemoryTracker::Get()->getAllocatedMemory(BruteForceRendererMetrics::NAME);
}

void BruteForceTerrainRenderer::Render(const Camera& camera)
{
	CommandBuffer->beginQuery(BruteForceRendererMetrics::RENDER_TERRAIN);

	VkCommandBuffer cmdBuffer = CommandBuffer->getCurrentCommandBuffer();
	VulkanRenderer::beginRenderPass(cmdBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(cmdBuffer, m_TerrainRenderPass, m_BruteForceLOD->getMostRecentIndex());

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(cmdBuffer, m_TerrainRenderPass.Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdDrawIndirect(cmdBuffer, m_BruteForceLOD->getIndirectDrawCommand()->getBuffer(), 0, 1, sizeof(VkDrawIndirectCommand));

	VulkanRenderer::endRenderPass(cmdBuffer);

	CommandBuffer->endQuery(BruteForceRendererMetrics::RENDER_TERRAIN);
}

void BruteForceTerrainRenderer::Refresh(const Camera& camera)
{
	CommandBuffer->beginQuery(BruteForceRendererMetrics::GPU_CREATE_CHUNK_BUFFER);
	m_BruteForceLOD->Generate(CommandBuffer->getCurrentCommandBuffer(), camera, DistanceThreshold);
	CommandBuffer->endQuery(BruteForceRendererMetrics::GPU_CREATE_CHUNK_BUFFER);
}

void BruteForceTerrainRenderer::setWireframe(bool wireframe)
{
	if (m_InWireframe != wireframe)
	{
		vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());

		m_InWireframe = wireframe;
		createPipeline();
	}
}

void BruteForceTerrainRenderer::createRenderPass()
{
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader(BRUTE_FORCE_TERRAIN_RENDER_SHADER_NAME);
		mainShader->addShaderStage(ShaderStage::VERTEX, BRUTE_FORCE_TERRAIN_RENDER_VERTEX_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::TESSELLATION_CONTROL, BRUTE_FORCE_TERRAIN_RENDER_TESSELLATION_CONTROL_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::TESSELLATION_EVALUATION, BRUTE_FORCE_TERRAIN_RENDER_TESSELLATION_EVALUATION_SHADER_PATH);
		mainShader->addShaderStage(ShaderStage::FRAGMENT, BRUTE_FORCE_TERRAIN_RENDER_FRAGMENT_SHADER_PATH);
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(BRUTE_FORCE_TERRAIN_RENDER_SHADER_NAME));
		DescriptorSet->bindInput(0, 0, 0, m_BruteForceLOD->ChunksToRender);
		DescriptorSet->bindInput(1, 0, 0, m_TerrainMap->getMap());
		DescriptorSet->bindInput(1, 1, 0, m_Terrain->TerrainInfoBuffer);
		DescriptorSet->Create();
		m_TerrainRenderPass.DescriptorSet = DescriptorSet;
	}
}

void BruteForceTerrainRenderer::createPipeline()
{
	PipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader(BRUTE_FORCE_TERRAIN_RENDER_SHADER_NAME);
	spec.vertexBufferLayout = VulkanVertexBufferLayout{ VertexType::INT_2 };
	spec.depthCompareFunction = DepthCompare::LESS;
	spec.Topology = PrimitiveTopology::PATCHES;
	spec.tessellationControlPoints = 4;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass.Pipeline = m_TerrainPipeline;
}

void BruteForceTerrainRenderer::createVertexBuffer()
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
