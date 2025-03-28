#include "ClipmapLOD.h"

#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Terrain/Clipmap/ClipmapData.h"

#include <memory>

#define CLIPMAP_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE "Terrain/Clipmap/ConstructTerrainChunks_comp.glsl"

ClipmapLOD::ClipmapLOD(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<TerrainClipmap>& clipmap)
	: m_TerrainSpecification(terrain->getSpecification())
{
	ClipmapTerrainSpecification clipmapSpec = clipmap->getSpecification();
	m_RingSize = clipmapSpec.ClipmapSize / m_TerrainSpecification.Info.ChunkSize;

	{
		VulkanBufferProperties LODMarginsProperties;
		LODMarginsProperties.Size = m_TerrainSpecification.Info.LODCount * ((uint32_t)sizeof(TerrainInfo));
		LODMarginsProperties.Type = BufferType::STORAGE_BUFFER;
		LODMarginsProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		LODMarginsBufferSet = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), LODMarginsProperties);
	}

	createChunkComputePass(terrain->TerrainInfoBuffer);
}

void ClipmapLOD::computeMargins(const glm::ivec2& cameraPosition)
{
	std::vector<LODMargins> margins;

	TerrainInfo tInfo = m_TerrainSpecification.Info;

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

		margins.push_back(LODMargins{ { minX, maxX }, { minY, maxY } });
	}

	LODMarginsBufferSet->getBuffer(m_NextBuffer)->setDataCPU(margins.data(), margins.size() * sizeof(LODMargins));

	m_CurrentlyUsedBuffer = m_NextBuffer;
	(++m_NextBuffer) %= VulkanRenderer::getFramesInFlight();
}

void ClipmapLOD::Generate(VkCommandBuffer commandBuffer, const Camera& cam)
{
	std::array<glm::vec4, 6> frustumPlanes = cam.getFrustum();
	m_FrustumBuffer->setDataCPU(&frustumPlanes, 6 * sizeof(glm::vec4));

	uint32_t dispatchCount = uint32_t(glm::ceil(float(m_RingSize) / 8.0f));

	VulkanRenderer::dispatchCompute(commandBuffer, m_ConstructTerrainChunksPass, m_CurrentlyUsedBuffer, { dispatchCount, dispatchCount, m_TerrainSpecification.Info.LODCount });
}

void ClipmapLOD::createChunkComputePass(const std::shared_ptr<VulkanBuffer>& infoBuffer)
{
	{
		VulkanBufferProperties chunkToRenderProperties;
		chunkToRenderProperties.Size = 1024 * (uint32_t)sizeof(TerrainChunk);
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
		renderProperties.Size = sizeof(VkDrawIndexedIndirectCommand);
		renderProperties.Type = BufferType::INDIRECT_BUFFER | BufferType::STORAGE_BUFFER;
		renderProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

		m_RenderCommand = std::make_shared<VulkanBuffer>(renderProperties);
	}

	{
		std::shared_ptr<VulkanShader>& frustumCullShader = ShaderManager::createShader(CLIPMAP_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE);
		frustumCullShader->addShaderStage(ShaderStage::COMPUTE, CLIPMAP_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE);
		frustumCullShader->createDescriptorSetLayouts();

		m_ConstructTerrainChunksPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(CLIPMAP_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE));
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 0, 0, ChunksToRender);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 1, 0, LODMarginsBufferSet);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(0, 2, 0, infoBuffer);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(1, 0, 0, m_FrustumBuffer);
		m_ConstructTerrainChunksPass.DescriptorSet->bindInput(2, 0, 0, m_RenderCommand);
		m_ConstructTerrainChunksPass.DescriptorSet->Create();

		m_ConstructTerrainChunksPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(CLIPMAP_CONSTRUCT_TERRAIN_CHUNKS_COMPUTE));
	}
}
