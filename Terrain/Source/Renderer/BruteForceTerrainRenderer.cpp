#include "BruteForceTerrainRenderer.h"

#include "Graphics/Vulkan/VulkanDevice.h"

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

	m_TerrainMap = std::make_shared<TerrainMap>(specification.HeightMapFilepath, 1024 * 4, 1024 * 4);
	m_BruteForceLOD = std::make_shared<BruteForceLOD>(m_Terrain);

	createVertexBuffer();

	createRenderPass();
	createPipeline();

	SimpleVulkanMemoryTracker::Get()->Stop();
	BruteForceRendererMetrics::MEMORY_USED = SimpleVulkanMemoryTracker::Get()->getAllocatedMemory(BruteForceRendererMetrics::NAME);
}

void BruteForceTerrainRenderer::Render(const Camera& camera, uint32_t frameIndex)
{
	VkCommandBuffer cmdBuffer = CommandBuffer->getCommandBuffer(frameIndex);

	CameraRenderMatrices matrices = camera.getRenderMatrices();

	CommandBuffer->beginTimeQuery(BruteForceRendererMetrics::RENDER_TERRAIN);

	m_TerrainRenderPass.Begin(cmdBuffer);
	m_TerrainRenderPass.Prepare(cmdBuffer, m_BruteForceLOD->getMostRecentIndex(), sizeof(CameraRenderMatrices), &matrices, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

	VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdDrawIndirect(cmdBuffer, m_BruteForceLOD->getIndirectDrawCommand()->getBuffer(), 0, 1, sizeof(VkDrawIndirectCommand));

	m_TerrainRenderPass.End(cmdBuffer);

	CommandBuffer->endTimeQuery(BruteForceRendererMetrics::RENDER_TERRAIN);
}

void BruteForceTerrainRenderer::Refresh(const Camera& camera, uint32_t frameIndex)
{
	CommandBuffer->beginTimeQuery(BruteForceRendererMetrics::GPU_CREATE_CHUNK_BUFFER);
	m_BruteForceLOD->Generate(CommandBuffer->getCommandBuffer(frameIndex), camera, DistanceThreshold);
	CommandBuffer->endTimeQuery(BruteForceRendererMetrics::GPU_CREATE_CHUNK_BUFFER);
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
	RenderPipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.Shader = ShaderManager::getShader(BRUTE_FORCE_TERRAIN_RENDER_SHADER_NAME);

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
