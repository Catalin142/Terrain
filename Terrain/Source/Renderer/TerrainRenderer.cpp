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
	if (m_LastTechniqueUsed != terrain->getCurrentTechnique())
	{
		m_LastTechniqueUsed = terrain->getCurrentTechnique();
		initializeRenderPass(terrain);
	}

	uint32_t currentFrame = VulkanRenderer::getCurrentFrame();

	m_CameraBufferSet->setData(&camera.getRenderMatrices(), sizeof(CameraRenderMatrices), currentFrame);

	switch (terrain->getCurrentTechnique())
	{
	case LODTechnique::DISTANCE_BASED:
		renderDistanceLOD(camera, terrain);
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
	m_TerrainChunksSet = std::make_shared<VulkanUniformBufferSet>(256 * sizeof(TerrainChunk), framesInFlight);
	m_LodMapSet = std::make_shared<VulkanUniformBufferSet>(16 * 16 * sizeof(LODLevel), framesInFlight);
	m_CameraBufferSet = std::make_shared<VulkanUniformBufferSet>((uint32_t)sizeof(CameraRenderMatrices), framesInFlight);
}

void TerrainRenderer::initializeRenderPass(const std::shared_ptr<Terrain>& terrain)
{
	switch (terrain->getCurrentTechnique())
	{
	case LODTechnique::DISTANCE_BASED:
		createDistanceLODRenderPass(terrain);
		break;
	}
}

void TerrainRenderer::createDistanceLODRenderPass(const std::shared_ptr<Terrain>& terrain)
{
	if (terrain->getDistanceLODTechnique() == nullptr)
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
		DescriptorSet->bindInput(0, 0, m_CameraBufferSet);
		DescriptorSet->bindInput(0, 1, m_TerrainChunksSet);
		DescriptorSet->bindInput(0, 2, m_LodMapSet);
		DescriptorSet->bindInput(1, 0, terrain->getDistanceLODTechnique()->getHeightMap());
		DescriptorSet->bindInput(2, 0, terrain->getDistanceLODTechnique()->getCompositionMap());
		DescriptorSet->bindInput(2, 1, terrain->getDistanceLODTechnique()->getTerrainTextures());

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
	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass->setPipeline(m_TerrainPipeline);
}

void TerrainRenderer::renderDistanceLOD(const Camera& camera, const std::shared_ptr<Terrain>& terrain)
{
	std::vector<TerrainChunk> chunksToRender = terrain->getChunksToRender(camera.getPosition());

	const std::shared_ptr<DistanceLOD>& distanceLOD = terrain->getDistanceLODTechnique();

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	m_CameraBufferSet->setData(&camera.getRenderMatrices(), sizeof(CameraRenderMatrices), m_CurrentFrame);
	m_TerrainChunksSet->setData(chunksToRender.data(), chunksToRender.size() * sizeof(TerrainChunk), m_CurrentFrame);
	m_LodMapSet->setData(distanceLOD->getLodMap().data(), 16 * 16 * sizeof(LODLevel), m_CurrentFrame);

	uint32_t firstInstance = 0;

	VkCommandBuffer commandBuffer = m_RenderCommandBuffer->getCurrentCommandBuffer();

	uint32_t instanceCount = 0;

	VulkanRenderer::beginRenderPass(m_RenderCommandBuffer, m_TerrainRenderPass);

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
