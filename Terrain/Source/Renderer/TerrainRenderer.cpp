#include "TerrainRenderer.h"

#include "Terrain/Techniques/DistanceLOD.h"

#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanUtils.h"

#include <cassert>

struct LODComputeConstants
{
	int32_t leavesCount;
	int32_t terrinSize;
};


TerrainRenderer::TerrainRenderer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer,
	const std::shared_ptr<Terrain>& terrain) : m_TargetFramebuffer(targetFramebuffer), m_Terrain(terrain)
{
	initializeBuffers();

	// Lod Map
	{
		std::shared_ptr<VulkanShader>& lodCompute = ShaderManager::createShader("_LODCompute");
		lodCompute->addShaderStage(ShaderStage::COMPUTE, "Terrain/LODMap_comp.glsl");
		lodCompute->createDescriptorSetLayouts();

		ImageSpecification LODMapSpecification;
		LODMapSpecification.Width = uint32_t(terrain->getInfo().TerrainSize.x) / terrain->getInfo().MinimumChunkSize;
		LODMapSpecification.Height = uint32_t(terrain->getInfo().TerrainSize.x) / terrain->getInfo().MinimumChunkSize;
		LODMapSpecification.Format = VK_FORMAT_R8_UINT;
		LODMapSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		LODMapSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		m_LODMap = std::make_shared<VulkanImage>(LODMapSpecification);
		m_LODMap->Create();

		m_LODMapComputePass = std::make_shared<VulkanComputePass>();
		m_LODMapComputePass->DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_LODCompute"));
		m_LODMapComputePass->DescriptorSet->bindInput(0, 0, 0, m_LODMap);
		m_LODMapComputePass->DescriptorSet->bindInput(0, 1, 0, m_TerrainChunksSet);
		m_LODMapComputePass->DescriptorSet->Create();
		m_LODMapComputePass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_LODCompute"),
			uint32_t(sizeof(LODComputeConstants)));
	}

	SamplerSpecification terrainSamplerSpec{};
	terrainSamplerSpec.addresMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	m_TerrainSampler = std::make_shared<VulkanSampler>(terrainSamplerSpec);

	std::vector<std::string> filepaths = {
		"Resources/Img/noiseMap.png",
	};

	TextureSpecification texSpec{};
	texSpec.CreateSampler = false;
	texSpec.GenerateMips = false;
	texSpec.Channles = 4;
	texSpec.LayerCount = (uint32_t)filepaths.size();
	texSpec.Filepath = filepaths;
	m_NoiseMap = std::make_shared<VulkanTexture>(texSpec);

	initializeRenderPass();
}

void TerrainRenderer::Render(const Camera& camera)
{
	uint32_t currentFrame = VulkanRenderer::getCurrentFrame();
	renderTerrain(camera);
}

void TerrainRenderer::setTargetFramebuffer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer)
{
	m_TargetFramebuffer = targetFramebuffer;
}

void TerrainRenderer::setWireframe(bool wireframe)
{
	if (m_InWireframe != wireframe)
	{
		m_InWireframe = wireframe;
		initializeRenderPass();
	}
}

void TerrainRenderer::initializeBuffers()
{
	uint32_t framesInFlight = VulkanRenderer::getFramesInFlight();

	// TODO: Get rid of hard coded values
	m_TerrainChunksSet = std::make_shared<VulkanUniformBufferSet>(uint32_t(8 * 8 * sizeof(TerrainChunk)), framesInFlight);
	m_TerrainInfo = std::make_shared<VulkanUniformBuffer>((uint32_t)sizeof(TerrainInfo));
	m_CameraInfo = std::make_shared<VulkanUniformBuffer>((uint32_t)sizeof(glm::vec4));
}

void TerrainRenderer::initializeRenderPass()
{
	vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());
	
	// TODO: Destory the one that is not used
	createQuadRenderPass();
	createCircleRenderPass();
}

void TerrainRenderer::createQuadRenderPass()
{
	m_TerrainQuadRenderPass = std::make_shared<RenderPass>();

	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("_TerrainQuadShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Terrain/TerrainQuad_vert.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Terrain/Terrain_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_TerrainQuadShader"));
		DescriptorSet->bindInput(0, 0, 0, m_TerrainChunksSet);
		DescriptorSet->bindInput(0, 1, 0, m_TerrainInfo);
		DescriptorSet->bindInput(0, 2, 0, m_LODMap);
		DescriptorSet->bindInput(1, 0, 0, m_TerrainSampler);
		DescriptorSet->bindInput(1, 1, 0, m_Terrain->getHeightMap());
		DescriptorSet->bindInput(1, 2, 0, m_Terrain->getCompositionMap());
		DescriptorSet->bindInput(1, 3, 0, m_Terrain->getNormalMap());
		DescriptorSet->bindInput(2, 0, 0, m_Terrain->getTerrainTextures());
		DescriptorSet->bindInput(2, 1, 0, m_Terrain->getNormalTextures());
		DescriptorSet->bindInput(2, 2, 0, m_NoiseMap);

		DescriptorSet->Create();
		m_TerrainQuadRenderPass->DescriptorSet = DescriptorSet;
	}

	createQuadPipeline();
}

void TerrainRenderer::createQuadPipeline()
{
	PipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader("_TerrainQuadShader");
	spec.vertexBufferLayout = VulkanVertexBufferLayout{};
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainQuadPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainQuadRenderPass->Pipeline = m_TerrainQuadPipeline;

#if DEBUG_TERRAIN_NORMALS == 1
	createNormalDebugRenderPass();
#endif
}

void TerrainRenderer::createCircleRenderPass()
{
	m_TerrainCircleRenderPass = std::make_shared<RenderPass>();

	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("_TerrainCircleShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Terrain/TerrainCircle_vert.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Terrain/Terrain_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_TerrainCircleShader"));
		DescriptorSet->bindInput(0, 0, 0, m_TerrainChunksSet);
		DescriptorSet->bindInput(0, 1, 0, m_TerrainInfo);
		DescriptorSet->bindInput(0, 2, 0, m_CameraInfo);
		DescriptorSet->bindInput(1, 0, 0, m_TerrainSampler);
		DescriptorSet->bindInput(1, 1, 0, m_Terrain->getHeightMap());
		DescriptorSet->bindInput(1, 2, 0, m_Terrain->getCompositionMap());
		DescriptorSet->bindInput(1, 3, 0, m_Terrain->getNormalMap());
		DescriptorSet->bindInput(2, 0, 0, m_Terrain->getTerrainTextures());
		DescriptorSet->bindInput(2, 1, 0, m_Terrain->getNormalTextures());
		DescriptorSet->bindInput(2, 2, 0, m_NoiseMap);

		DescriptorSet->Create();
		m_TerrainCircleRenderPass->DescriptorSet = DescriptorSet;
	}

	createCirclePipeline();
}

void TerrainRenderer::createCirclePipeline()
{
	PipelineSpecification spec{};
	spec.Framebuffer = m_TargetFramebuffer;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = m_InWireframe;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader("_TerrainCircleShader");
	spec.vertexBufferLayout = VulkanVertexBufferLayout{};
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainCirclePipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainCircleRenderPass->Pipeline = m_TerrainCirclePipeline;

#if DEBUG_TERRAIN_NORMALS == 1
	createNormalDebugRenderPass();
#endif
}

void TerrainRenderer::renderTerrain(const Camera& camera)
{
	std::vector<TerrainChunk> chunksToRender = m_Terrain->getChunksToRender(camera.getPosition());

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	m_TerrainChunksSet->setData(chunksToRender.data(), chunksToRender.size() * sizeof(TerrainChunk), m_CurrentFrame);

	// compute LODMap in compute shader
	if (m_Terrain->getCurrentTechnique() != LODTechnique::SINKING_CIRCLE)
	{
		LODComputeConstants LODpc;
		LODpc.leavesCount = (int32_t)chunksToRender.size();
		LODpc.terrinSize = m_Terrain->getInfo().MinimumChunkSize;

		VulkanRenderer::dispatchCompute(m_RenderCommandBuffer, m_LODMapComputePass, { 1, 1, 1 },
			sizeof(LODComputeConstants), &LODpc);

		m_LODMapComputePass->Pipeline->imageMemoryBarrier(m_RenderCommandBuffer, m_LODMap, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
	}

	TerrainInfo terrainInfo = m_Terrain->getInfo();
	//terrainInfo.CamPos = { camera.getPosition().x, camera.getPosition().z };
	m_TerrainInfo->setData(&terrainInfo, sizeof(TerrainInfo));

	glm::vec4 cameraPos = { camera.getPosition().x, camera.getPosition().z, 0.0f, 0.0f};
	m_CameraInfo->setData(&cameraPos, sizeof(cameraPos));

	VkCommandBuffer commandBuffer = m_RenderCommandBuffer->getCurrentCommandBuffer();

	std::shared_ptr<RenderPass> currentPass = m_TerrainQuadRenderPass;
	if (m_Terrain->getCurrentTechnique() == LODTechnique::SINKING_CIRCLE)
		currentPass = m_TerrainCircleRenderPass;

	VulkanRenderer::beginRenderPass(m_RenderCommandBuffer, currentPass);
	VulkanRenderer::preparePipeline(m_RenderCommandBuffer, currentPass);

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(commandBuffer, currentPass->Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	uint32_t instanceCount = 0;
	switch (m_Terrain->getCurrentTechnique())
	{
	case LODTechnique::DISTANCE_BASED:

		for (const LODRange& props : m_Terrain->getDistanceLODTechnique()->getLODs())
		{
			if (props.CurrentCount)
			{
				TerrainChunkIndexBuffer idxBuffer = getChunkIndexBufferLOD(props.LOD);

				vkCmdBindIndexBuffer(commandBuffer, idxBuffer.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(idxBuffer.IndicesCount), props.CurrentCount, 0, 0, instanceCount);
				instanceCount += props.CurrentCount;
			}
		}

		break;

	case LODTechnique::SINKING_CIRCLE:
	case LODTechnique::QUAD_TREE:
		TerrainChunkIndexBuffer idxBuffer = getChunkIndexBufferLOD(1);

		vkCmdBindIndexBuffer(commandBuffer, idxBuffer.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, idxBuffer.IndicesCount, (uint32_t)chunksToRender.size(), 0, 0, 0);

		break;
	}



#if DEBUG_TERRAIN_NORMALS == 1
	VulkanRenderer::preparePipeline(m_RenderCommandBuffer, m_NormalsDebugRenderPass);

	vkCmdPushConstants(commandBuffer, m_NormalsDebugRenderPass->Pipeline->getVkPipelineLayout(),
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	instanceCount = 0;
	for (const LODRange& props : m_Terrain->getDistanceLODTechnique()->getLODs())
	{
		if (props.CurrentCount)
		{
			TerrainChunkIndexBuffer idxBuffer = getChunkIndexBufferLOD(props.LOD);

			vkCmdBindIndexBuffer(commandBuffer, idxBuffer.IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, uint32_t(idxBuffer.IndicesCount), props.CurrentCount, 0, 0, instanceCount);
			instanceCount += props.CurrentCount;
		}
	}
#endif

	VulkanRenderer::endRenderPass(m_RenderCommandBuffer);

}

const TerrainChunkIndexBuffer& TerrainRenderer::getChunkIndexBufferLOD(uint8_t lod)
{
	if (m_LODIndexBuffer.find(lod) != m_LODIndexBuffer.end())
		return m_LODIndexBuffer[lod];

	uint32_t vertCount = m_Terrain->getInfo().MinimumChunkSize + 1;
	std::vector<uint32_t> indices = TerrainChunk::generateIndices(lod, vertCount);

	TerrainChunkIndexBuffer current;

	current.IndicesCount = (uint32_t)indices.size();
	current.IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
		BufferType::INDEX, BufferUsage::STATIC);
	
	m_LODIndexBuffer[lod] = current;

	return m_LODIndexBuffer[lod];
}

#if DEBUG_TERRAIN_NORMALS == 1
void TerrainRenderer::createNormalDebugRenderPass()
{
	std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("debugNormals");
	mainShader->addShaderStage(ShaderStage::VERTEX, "Debug/TerrainNormals_vert.glsl");
	mainShader->addShaderStage(ShaderStage::GEOMETRY, "Debug/TerrainNormals_geom.glsl");
	mainShader->addShaderStage(ShaderStage::FRAGMENT, "Debug/TerrainNormals_frag.glsl");
	mainShader->createDescriptorSetLayouts();

	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
	DescriptorSet = std::make_shared<VulkanDescriptorSet>(mainShader);
	DescriptorSet->bindInput(0, 0, 0, m_TerrainChunksSet);
	DescriptorSet->bindInput(0, 1, 0, m_TerrainInfo);
	DescriptorSet->bindInput(1, 0, 0, m_TerrainSampler);
	DescriptorSet->bindInput(1, 1, 0, m_Terrain->getHeightMap());
	DescriptorSet->bindInput(1, 2, 0, m_Terrain->getNormalMap());
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
#endif
