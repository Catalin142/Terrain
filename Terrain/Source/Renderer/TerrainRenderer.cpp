#include "TerrainRenderer.h"

#include "Terrain/Techniques/DistanceLOD.h"

#include "Graphics/VulkanRenderer.h"
#include "Graphics/VulkanDevice.h"

#include <cassert>

TerrainRenderer::TerrainRenderer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer) : m_TargetFramebuffer(targetFramebuffer)
{
	initializeBuffers();
}

void TerrainRenderer::renderTerrain(const Camera& camera, const std::shared_ptr<Terrain>& terrain)
{
	if (m_LastTechniqueUsed != terrain->getCurrentTechnique() || terrain != m_Terrain)
	{
		m_Terrain = terrain;
		m_LastTechniqueUsed = m_Terrain->getCurrentTechnique();
		initializeRenderPass();
	}

	uint32_t currentFrame = VulkanRenderer::getCurrentFrame();

	switch (m_Terrain->getCurrentTechnique())
	{
	case LODTechnique::DISTANCE_BASED:
		renderDistanceLOD(camera);
		break;
	}
}

void TerrainRenderer::onResize(uint32_t width, uint32_t height)
{
	m_TerrainRenderPass->Resize(width, height);
}

void TerrainRenderer::setWireframe(bool wireframe)
{
	if (m_InWireframe != wireframe)
	{
		m_InWireframe = wireframe;
		
		// Needs to be here so that GPU finish before recreating the Pipeline
		vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());

		switch (m_LastTechniqueUsed)
		{
		case LODTechnique::DISTANCE_BASED:
			createDistanceLODPipeline();
			break;
		}
	}
}

void TerrainRenderer::initializeBuffers()
{
	uint32_t framesInFlight = VulkanRenderer::getFramesInFlight();

	// TODO: Get rid of hard coded values
	m_TerrainChunksSet = std::make_shared<VulkanUniformBufferSet>(4096 * sizeof(TerrainChunk), framesInFlight);
	m_LodMapSet = std::make_shared<VulkanUniformBufferSet>(128 * 128 * sizeof(LODLevel), framesInFlight);
	m_TerrainInfo = std::make_shared<VulkanUniformBuffer>(4 * sizeof(float));
}

void TerrainRenderer::initializeRenderPass()
{
	switch (m_Terrain->getCurrentTechnique())
	{
	case LODTechnique::DISTANCE_BASED:
		createDistanceLODRenderPass();
		break;
	}
}

void TerrainRenderer::createDistanceLODRenderPass()
{
	if (m_Terrain->getDistanceLODTechnique() == nullptr)
		assert(false);

	m_TerrainRenderPass = std::make_shared<VulkanRenderPass>();

	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("DistanceLODShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "DistanceLOD_vert.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "DistanceLOD_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("DistanceLODShader"));
		DescriptorSet->bindInput(0, 0, m_TerrainChunksSet);
		DescriptorSet->bindInput(0, 1, m_LodMapSet);
		DescriptorSet->bindInput(0, 2, m_TerrainInfo);
		DescriptorSet->bindInput(1, 0, m_Terrain->getDistanceLODTechnique()->getHeightMap());
		DescriptorSet->bindInput(2, 0, m_Terrain->getDistanceLODTechnique()->getCompositionMap());
		DescriptorSet->bindInput(2, 1, m_Terrain->getDistanceLODTechnique()->getTerrainTextures());

		DescriptorSet->Create();
		m_TerrainRenderPass->setDescriptorSet(DescriptorSet);
	}

	createDistanceLODPipeline();
}

void TerrainRenderer::createDistanceLODPipeline()
{
	PipelineSpecification spec;
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader("DistanceLODShader");
	spec.vertexBufferLayout = VulkanVertexBufferLayout{};
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass->setPipeline(m_TerrainPipeline);
}

void TerrainRenderer::renderDistanceLOD(const Camera& camera)
{
	std::vector<TerrainChunk> chunksToRender = m_Terrain->getChunksToRender(camera.getPosition());

	const std::shared_ptr<DistanceLOD>& distanceLOD = m_Terrain->getDistanceLODTechnique();

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	m_TerrainChunksSet->setData(chunksToRender.data(), chunksToRender.size() * sizeof(TerrainChunk), m_CurrentFrame);
	m_LodMapSet->setData(distanceLOD->getLodMap().data(), chunksToRender.size() * sizeof(LODLevel), m_CurrentFrame);
	m_TerrainInfo->setData(&m_Terrain->getInfo(), 4 * sizeof(float));

	uint32_t firstInstance = 0;

	VkCommandBuffer commandBuffer = m_RenderCommandBuffer->getCurrentCommandBuffer();

	uint32_t instanceCount = 0;

	VulkanRenderer::beginRenderPass(m_RenderCommandBuffer, m_TerrainRenderPass);

	vkCmdPushConstants(commandBuffer, m_TerrainRenderPass->getVulkanPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &camera.getRenderMatrices());

	for (const LODProperties& props : distanceLOD->getLODProperties())
	{
		if (props.CurrentCount)
		{
			vkCmdBindIndexBuffer(commandBuffer, props.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, uint32_t(props.IndicesCount), props.CurrentCount, 0, 0, instanceCount);
			instanceCount += props.CurrentCount;
		}
	}

	VulkanRenderer::endRenderPass(m_RenderCommandBuffer);
}
