#include "TerrainRenderer.h"

#include "Terrain/Techniques/DistanceLOD.h"

#include "Graphics/VulkanRenderer.h"
#include "Graphics/VulkanDevice.h"
#include "Graphics/VulkanUtils.h"

#include <cassert>

TerrainRenderer::TerrainRenderer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer) : m_TargetFramebuffer(targetFramebuffer)
{
	initializeBuffers();

	SamplerSpecification terrainSamplerSpec{};
	terrainSamplerSpec.addresMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	m_TerrainSampler = std::make_shared<VulkanSampler>(terrainSamplerSpec);
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
	m_TerrainRenderPass->Pipeline->getTargetFramebuffer()->Resize(width, height);
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
	m_TerrainChunksSet = std::make_shared<VulkanUniformBufferSet>(8 * 8 * sizeof(TerrainChunk), framesInFlight);
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

	m_TerrainRenderPass = std::make_shared<RenderPass>();

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
		DescriptorSet->bindInput(1, 0, m_TerrainSampler);
		DescriptorSet->bindInput(1, 1, m_Terrain->getHeightMap());
		DescriptorSet->bindInput(1, 2, m_Terrain->getCompositionMap());
		DescriptorSet->bindInput(1, 3, m_Terrain->getNormalMap());
		DescriptorSet->bindInput(2, 0, m_Terrain->getTerrainTextures());

		DescriptorSet->Create();
		m_TerrainRenderPass->DescriptorSet = DescriptorSet;
	}

	createDistanceLODPipeline();
}

void TerrainRenderer::createDistanceLODPipeline()
{
	PipelineSpecification spec{};
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

	m_TerrainRenderPass->Pipeline = m_TerrainPipeline;

#if DEBUG_TERRAIN_NORMALS == 1
	createNormalDebugRenderPass();
#endif
}

void TerrainRenderer::renderDistanceLOD(const Camera& camera)
{
	std::vector<TerrainChunk> chunksToRender = m_Terrain->getChunksToRender(camera.getPosition());

	const std::shared_ptr<DistanceLOD>& distanceLOD = m_Terrain->getDistanceLODTechnique();

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	m_TerrainChunksSet->setData(chunksToRender.data(), chunksToRender.size() * sizeof(TerrainChunk), m_CurrentFrame);
	m_LodMapSet->setData(distanceLOD->getLodMap().data(), chunksToRender.size() * sizeof(LODLevel), m_CurrentFrame);

	TerrainInfo terrainInfo = m_Terrain->getInfo();
	m_TerrainInfo->setData(&terrainInfo, 4 * sizeof(float));

	VkCommandBuffer commandBuffer = m_RenderCommandBuffer->getCurrentCommandBuffer();

	VulkanRenderer::beginRenderPass(m_RenderCommandBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(m_RenderCommandBuffer, m_TerrainRenderPass);

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(commandBuffer, m_TerrainRenderPass->Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	uint32_t instanceCount = 0;
	for (const LODProperties& props : distanceLOD->getLODProperties())
	{
		if (props.CurrentCount)
		{
			vkCmdBindIndexBuffer(commandBuffer, props.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, uint32_t(props.IndicesCount), props.CurrentCount, 0, 0, instanceCount);
			instanceCount += props.CurrentCount;
		}
	}


#if DEBUG_TERRAIN_NORMALS == 1
	VulkanRenderer::preparePipeline(m_RenderCommandBuffer, m_NormalsDebugRenderPass);

	vkCmdPushConstants(commandBuffer, m_NormalsDebugRenderPass->Pipeline->getVkPipelineLayout(),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	instanceCount = 0;
	for (const LODProperties& props : m_Terrain->m_DistanceLOD->getLODProperties())
	{
		if (props.CurrentCount)
		{
			vkCmdBindIndexBuffer(commandBuffer, props.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, uint32_t(props.IndicesCount), props.CurrentCount, 0, 0, instanceCount);
			instanceCount += props.CurrentCount;
		}
	}
#endif

	VulkanRenderer::endRenderPass(m_RenderCommandBuffer);

}

#if DEBUG_TERRAIN_NORMALS == 1
void TerrainRenderer::createNormalDebugRenderPass()
{
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("debugNormals");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Debug/TerrainNormals_vert.glsl");
		mainShader->addShaderStage(ShaderStage::GEOMETRY, "Debug/TerrainNormals_geom.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Debug/TerrainNormals_frag.glsl");
		mainShader->createDescriptorSetLayouts();

		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(mainShader);
		DescriptorSet->bindInput(0, 0, m_TerrainChunksSet);
		DescriptorSet->bindInput(0, 1, m_TerrainInfo);
		DescriptorSet->bindInput(1, 0, m_TerrainSampler);
		DescriptorSet->bindInput(1, 1, m_Terrain->getHeightMap());
		DescriptorSet->bindInput(1, 2, m_Terrain->getNormalMap());
		DescriptorSet->Create();

		PipelineSpecification spec{};
		spec.lineWidth = 1.0f;
		spec.Framebuffer = m_TargetFramebuffer;
		spec.Shader = mainShader;
		spec.depthTest = true;
		spec.depthCompareFunction = DepthCompare::LESS;
		spec.vertexBufferLayout = VulkanVertexBufferLayout{};

		spec.pushConstants.push_back({ sizeof(CameraRenderMatrices),
			VkShaderStageFlagBits(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT) });

		std::shared_ptr<VulkanPipeline> pipelineDebug = std::make_shared<VulkanPipeline>(spec);

		m_NormalsDebugRenderPass = std::make_shared<RenderPass>();
		m_NormalsDebugRenderPass->DescriptorSet = DescriptorSet;
		m_NormalsDebugRenderPass->Pipeline = pipelineDebug;
	}
}
#endif
