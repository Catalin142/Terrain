#include "TerrainGenerator.h"

#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanUtils.h"

TerrainGenerator::TerrainGenerator(uint32_t width, uint32_t height) : m_Width(width), m_Height(height)
{
	{
		VulkanBufferProperties uniformBufferProperties;
		uniformBufferProperties.Size = (uint32_t)sizeof(glm::vec4);
		uniformBufferProperties.Type = BufferType::UNIFORM_BUFFER;
		uniformBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;
		m_UniformBuffer = std::make_shared<VulkanBuffer>(uniformBufferProperties);
	}

	createShaders();
	createImages();
	createCompute();
}

void TerrainGenerator::Generate(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer)
{
	if (!m_Valid)
	{
		MemoryBarrierSpecification barrierComputeComputeSpecification;
		barrierComputeComputeSpecification.fromAccess = VK_ACCESS_SHADER_WRITE_BIT;
		barrierComputeComputeSpecification.fromStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		barrierComputeComputeSpecification.toAccess = VK_ACCESS_SHADER_READ_BIT;
		barrierComputeComputeSpecification.toStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		MemoryBarrierSpecification barrierComputeFragmentSpecification;
		barrierComputeFragmentSpecification.fromAccess = VK_ACCESS_SHADER_WRITE_BIT;
		barrierComputeFragmentSpecification.fromStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		barrierComputeFragmentSpecification.toAccess = VK_ACCESS_SHADER_READ_BIT;
		barrierComputeFragmentSpecification.toStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		VkCommandBuffer currentBuffer = commandBuffer->getCurrentCommandBuffer();
		HydraulicSimulations = 0;
		{
			VulkanRenderer::dispatchCompute(currentBuffer, m_NoiseGenerationPass, { m_Width / 8, m_Height / 8, 1 },
				sizeof(GenerationParameters), &Noise);

			// Prevents the composition pass to read noise and normals before they are finished
			VulkanUtils::imageMemoryBarrier(currentBuffer, m_Noise, barrierComputeComputeSpecification);
		}

		{
			VulkanRenderer::dispatchCompute(currentBuffer, m_HydraulicErosionPass, { 1, 1, 1 });

			VulkanUtils::imageMemoryBarrier(currentBuffer, m_Noise, barrierComputeComputeSpecification);
			VulkanUtils::imageMemoryBarrier(currentBuffer, m_Noise, barrierComputeFragmentSpecification);
		}

		// compute normals
		{
			VulkanRenderer::dispatchCompute(currentBuffer, m_NormalComputePass, { m_Width / 8, m_Height / 8, 1 });

			VulkanUtils::imageMemoryBarrier(currentBuffer, m_Normals, barrierComputeComputeSpecification);
			VulkanUtils::imageMemoryBarrier(currentBuffer, m_Normals, barrierComputeFragmentSpecification);
		}

		{
			VulkanRenderer::dispatchCompute(currentBuffer, m_CompositionPass, { m_Width / 8, m_Height / 8, 1 });

			// prevernts terrain fragment shader read from composition image until it is finished
			VulkanUtils::imageMemoryBarrier(currentBuffer, m_Composition, barrierComputeFragmentSpecification);
		}

		m_Valid = true;
	}

	if (RunHydraulicErosion)
		runHydraulicErosion(commandBuffer);
}

void TerrainGenerator::Resize(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;
	createImages();
	m_Valid = false;
}

void TerrainGenerator::runHydraulicErosion(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer)
{
	static float t = 0.0f;
	static float r = 0.0f;
	t = ((float)rand() / (float)RAND_MAX) * 17323.0f;
	r = ((float)rand() / (float)RAND_MAX) * 238727.0f;
	glm::vec2 a{ t, r };
	m_UniformBuffer->setDataCPU(&a, sizeof(glm::vec2));

	VkCommandBuffer currentBuffer = commandBuffer->getCurrentCommandBuffer();

	MemoryBarrierSpecification barrierComputeComputeSpecification;
	barrierComputeComputeSpecification.fromAccess = VK_ACCESS_SHADER_WRITE_BIT;
	barrierComputeComputeSpecification.fromStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	barrierComputeComputeSpecification.toAccess = VK_ACCESS_SHADER_READ_BIT;
	barrierComputeComputeSpecification.toStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	MemoryBarrierSpecification barrierComputeFragmentSpecification;
	barrierComputeComputeSpecification.fromAccess = VK_ACCESS_SHADER_WRITE_BIT;
	barrierComputeComputeSpecification.fromStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	barrierComputeComputeSpecification.toAccess = VK_ACCESS_SHADER_READ_BIT;
	barrierComputeComputeSpecification.toStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	{
		HydraulicSimulations += 1024;
		VulkanRenderer::dispatchCompute(currentBuffer, m_HydraulicErosionPass, { 1, 1, 1 },
			sizeof(HydraulicErosionParameters), &HydraulicErosion);

		VulkanUtils::imageMemoryBarrier(currentBuffer, m_Noise, barrierComputeComputeSpecification);
		VulkanUtils::imageMemoryBarrier(currentBuffer, m_Noise, barrierComputeFragmentSpecification);
	}

	// compute normals
	{
		VulkanRenderer::dispatchCompute(currentBuffer, m_NormalComputePass, { m_Width / 8, m_Height / 8, 1 });

		VulkanUtils::imageMemoryBarrier(currentBuffer, m_Normals, barrierComputeComputeSpecification);
		VulkanUtils::imageMemoryBarrier(currentBuffer, m_Normals, barrierComputeFragmentSpecification);
	}

	{
		VulkanRenderer::dispatchCompute(currentBuffer, m_CompositionPass, { m_Width / 8, m_Height / 8, 1 });

		// prevernts terrain fragment shader read from composition image until its finished
		VulkanUtils::imageMemoryBarrier(currentBuffer, m_Composition, barrierComputeFragmentSpecification);
	}
}

void TerrainGenerator::notifyChange()
{
	m_Valid = false;
}

void TerrainGenerator::createShaders()
{
	std::shared_ptr<VulkanShader>& noiseCompute = ShaderManager::createShader("_TerrainGenerator");
	noiseCompute->addShaderStage(ShaderStage::COMPUTE, "TerrainGeneration/TerrainGenerator_comp.glsl");
	noiseCompute->createDescriptorSetLayouts();

	std::shared_ptr<VulkanShader>& normalCompute = ShaderManager::createShader("_NormalCompute");
	normalCompute->addShaderStage(ShaderStage::COMPUTE, "TerrainGeneration/TerrainNormals_comp.glsl");
	normalCompute->createDescriptorSetLayouts();

	std::shared_ptr<VulkanShader>& compositionCompute = ShaderManager::createShader("_CompositionCompute");
	compositionCompute->addShaderStage(ShaderStage::COMPUTE, "TerrainGeneration/TerrainComposition_comp.glsl");
	compositionCompute->createDescriptorSetLayouts();

	std::shared_ptr<VulkanShader>& hydraulicErosionCompute = ShaderManager::createShader("_HydraulicErostionCompute");
	hydraulicErosionCompute->addShaderStage(ShaderStage::COMPUTE, "TerrainGeneration/HydraulicErosion_comp.glsl");
	hydraulicErosionCompute->createDescriptorSetLayouts();
}

void TerrainGenerator::createImages()
{
	{
		VulkanImageSpecification noiseHeightmapSpecification;
		noiseHeightmapSpecification.Width = m_Width;
		noiseHeightmapSpecification.Height = m_Height;
		noiseHeightmapSpecification.Format = VK_FORMAT_R16_SFLOAT;
		noiseHeightmapSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		noiseHeightmapSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		noiseHeightmapSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		noiseHeightmapSpecification.Mips = 4;
		m_Noise = std::make_shared<VulkanImage>(noiseHeightmapSpecification);
		m_Noise->Create();
	}

	{
		VulkanImageSpecification normalSpecification;
		normalSpecification.Width = m_Width;
		normalSpecification.Height = m_Height;
		normalSpecification.Format = VK_FORMAT_R8G8B8A8_SNORM;
		normalSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		normalSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_Normals = std::make_shared<VulkanImage>(normalSpecification);
		m_Normals->Create();
	}

	{
		VulkanImageSpecification compositionSpecification;
		compositionSpecification.Width = m_Width;
		compositionSpecification.Height = m_Height;
		compositionSpecification.Format = VK_FORMAT_R8_UINT;
		compositionSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		compositionSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_Composition = std::make_shared<VulkanImage>(compositionSpecification);
		m_Composition->Create();
	}
}

void TerrainGenerator::createCompute()
{
	{
		m_NoiseGenerationPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_TerrainGenerator"));
		m_NoiseGenerationPass.DescriptorSet->bindInput(0, 0, 0, m_Noise);
		m_NoiseGenerationPass.DescriptorSet->Create();
		m_NoiseGenerationPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_TerrainGenerator"), 
			uint32_t(sizeof(GenerationParameters)));
	}

	{
		m_NormalComputePass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_NormalCompute"));
		m_NormalComputePass.DescriptorSet->bindInput(0, 0, 0, m_Noise);
		m_NormalComputePass.DescriptorSet->bindInput(0, 1, 0, m_Normals);
		m_NormalComputePass.DescriptorSet->Create();
		m_NormalComputePass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_NormalCompute"));
	}

	{
		m_CompositionPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_CompositionCompute"));
		m_CompositionPass.DescriptorSet->bindInput(0, 0, 0, m_Normals);
		m_CompositionPass.DescriptorSet->bindInput(0, 1, 0, m_Composition);
		m_CompositionPass.DescriptorSet->Create();
		m_CompositionPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_CompositionCompute"));
	}

	{
		m_HydraulicErosionPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_HydraulicErostionCompute"));
		m_HydraulicErosionPass.DescriptorSet->bindInput(0, 0, 0, m_Noise);
		m_HydraulicErosionPass.DescriptorSet->bindInput(1, 0, 0, m_UniformBuffer);
		m_HydraulicErosionPass.DescriptorSet->Create();
		m_HydraulicErosionPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_HydraulicErostionCompute"),
			uint32_t(sizeof(HydraulicErosionParameters)));
	}
}
