#include "TessellationLOD.h"

#include "Graphics/Vulkan/VulkanRenderer.h"

#define TESSSELLATION_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE "Terrain/ClipmapTessellation/ConstructTerrainChunks_comp.glsl"

enum STITCH_DIRECTION
{
	LEFT	= 1 << 0,
	RIGHT	= 1 << 1,
	DOWN	= 1 << 2,
	UP		= 1 << 3,
};

TessellationLOD::TessellationLOD(const TerrainSpecification& spec, const std::shared_ptr<TerrainClipmap>& clipmap)
	: m_TerrainSpecification(spec)
{
	ClipmapTerrainSpecification clipmapSpec = clipmap->getSpecification();
	m_RingSize = clipmapSpec.ClipmapSize / spec.Info.ChunkSize;
	
	{
		VulkanBufferProperties LODMarginsProperties;
		LODMarginsProperties.Size = m_TerrainSpecification.Info.LODCount * ((uint32_t)sizeof(TerrainInfo));
		LODMarginsProperties.Type = BufferType::STORAGE_BUFFER;
		LODMarginsProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		LODMarginsBufferSet = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), LODMarginsProperties);
	}

}

void TessellationLOD::computeMargins(const glm::ivec2& cameraPosition, uint32_t patchSize)
{
	std::vector<TerrainChunk> chunks;
	std::vector<LODMargins> margins;

	TerrainInfo tInfo = m_TerrainSpecification.Info;

	uint32_t multiplier = tInfo.ChunkSize / patchSize;

	for (int32_t lod = 0; lod < tInfo.LODCount; lod++)
	{
		TerrainChunk tc;
		tc.Lod = lod;

		int32_t chunkSize = tInfo.ChunkSize * (1 << lod);

		glm::ivec2 newCamPos = cameraPosition;
		newCamPos.x = glm::max(newCamPos.x, chunkSize * (m_RingSize / 2));
		newCamPos.y = glm::max(newCamPos.y, chunkSize * (m_RingSize / 2));
		newCamPos.x = glm::min(newCamPos.x, tInfo.TerrainSize - chunkSize * (m_RingSize / 2));
		newCamPos.y = glm::min(newCamPos.y, tInfo.TerrainSize - chunkSize * (m_RingSize / 2));

		glm::ivec2 snapedPosition;
		snapedPosition.x = int32_t(newCamPos.x) / chunkSize;
		snapedPosition.y = int32_t(newCamPos.y) / chunkSize;

		int32_t minY = glm::max(int32_t(snapedPosition.y) - m_RingSize / 2, 0);
		int32_t maxY = glm::min(int32_t(snapedPosition.y) + m_RingSize / 2, tInfo.TerrainSize / chunkSize);

		int32_t minX = glm::max(int32_t(snapedPosition.x) - m_RingSize / 2, 0);
		int32_t maxX = glm::min(int32_t(snapedPosition.x) + m_RingSize / 2, tInfo.TerrainSize / chunkSize);

		if (snapedPosition.x % 2 != 0)
		{
			minX -= 1;
			maxX -= 1;
		}

		if (snapedPosition.y % 2 != 0)
		{
			minY -= 1;
			maxY -= 1;
		}

		margins.push_back(LODMargins{ { minX * multiplier, maxX * multiplier}, { minY * multiplier, maxY * multiplier} });
	}

	LODMarginsBufferSet->getBuffer(m_NextBuffer)->setDataCPU(margins.data(), margins.size() * sizeof(LODMargins));

	m_CurrentlyUsedBuffer = m_NextBuffer;
	(++m_NextBuffer) %= VulkanRenderer::getFramesInFlight();
}

void TessellationLOD::Generate(VkCommandBuffer commandBuffer, const Camera& cam, uint32_t patchSize)
{
	std::array<glm::vec4, 6> frustumPlanes = cam.getFrustum();
	m_FrustumBuffer->setDataCPU(&frustumPlanes, 6 * sizeof(glm::vec4));

	TerrainInfo tInfo = m_TerrainSpecification.Info;
	uint32_t multiplier = tInfo.ChunkSize / patchSize;

	uint32_t dispatchCount = uint32_t(glm::ceil(float(m_RingSize * multiplier) / 8.0f));

	VulkanRenderer::dispatchCompute(commandBuffer, m_ConstructTerrainChunksPass, m_CurrentlyUsedBuffer, { dispatchCount, dispatchCount, m_TerrainSpecification.Info.LODCount });
}

void TessellationLOD::createResources(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<VulkanBuffer>& tessSettings)
{
	{
		VulkanBufferProperties chunkToRenderProperties;
		chunkToRenderProperties.Size = 8 * 1024 * (uint32_t)sizeof(TerrainChunk);
		chunkToRenderProperties.Type = BufferType::STORAGE_BUFFER;
		chunkToRenderProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		ChunksToRender = std::make_shared<VulkanBuffer>(chunkToRenderProperties);
	}
	{
		VulkanBufferProperties frustumBufferProperties;
		frustumBufferProperties.Size = ((uint32_t)sizeof(glm::vec4)) * 6;
		frustumBufferProperties.Type = BufferType::STORAGE_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		frustumBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		m_FrustumBuffer = std::make_shared<VulkanBuffer>(frustumBufferProperties);
	}
	{
		VulkanBufferProperties renderProperties;
		renderProperties.Size = sizeof(VkDrawIndirectCommand);
		renderProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		renderProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		RenderCommand = std::make_shared<VulkanBuffer>(renderProperties);
	}

	{
		std::shared_ptr<VulkanShader>& frustumCullShader = ShaderManager::createShader(TESSSELLATION_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE);
		frustumCullShader->addShaderStage(ShaderStage::COMPUTE, TESSSELLATION_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE);
		frustumCullShader->createDescriptorSetLayouts();

		m_ConstructTerrainChunksPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(TESSSELLATION_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE));
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 0, 0, ChunksToRender);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 1, 0, LODMarginsBufferSet);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 2, 0, tessSettings);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 3, 0, terrain->TerrainInfoBuffer);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(1, 0, 0, m_FrustumBuffer);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(2, 0, 0, RenderCommand);
		m_ConstructTerrainChunksPass.DescriptorSet->Create();

		m_ConstructTerrainChunksPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(TESSSELLATION_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE));
	}
}