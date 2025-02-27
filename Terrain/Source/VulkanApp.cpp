#include "VulkanApp.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>

#include "Core/Instrumentor.h"

#include "Graphics/Vulkan/VulkanUtils.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include <imgui.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <chrono>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Graphics/Vulkan/VulkanShader.h"
#include "Terrain/VirtualMap/DynamicVirtualTerrainDeserializer.h"
#include "Terrain/VirtualMap/VirtualTerrainSerializer.h"
#include "Terrain/VirtualMap/VirtualMapData.h"
#include <backends/imgui_impl_vulkan.h>

#include "GUI/ProfilerGUI.h"

#include <future>
#include "System/Disk.h"

/*
- Quad tree abstraction
- Change descriptor set creation, i don t need two identical descriptors if i don t use a bufferset. Also, i want to force some pipelines to use different descritors
- Change how image barriers are implemented, i want to put barries on all mips
*/

void SwapBuffers(VkCommandBuffer cmd, const std::shared_ptr<VulkanBuffer>& a, const std::shared_ptr<VulkanBuffer>& b, int howMuch);

VulkanApp::VulkanApp(const std::string& title, uint32_t width, uint32_t height) : Application(title, width, height)
{ }

void VulkanApp::onCreate()
{
	CommandBuffer = std::make_shared<VulkanRenderCommandBuffer>(true);

	{
		FramebufferSpecification framebufferSpecification;
		framebufferSpecification.Width = 1600;
		framebufferSpecification.Height = 900;
		framebufferSpecification.Samples = 1;
		framebufferSpecification.Clear = true;

		framebufferSpecification.DepthAttachment.Format = VK_FORMAT_D32_SFLOAT;
		framebufferSpecification.DepthAttachment.Blend = false;
		framebufferSpecification.DepthAttachment.IntialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		framebufferSpecification.DepthAttachment.FinalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		FramebufferAttachment outputColorAttachment{};
		outputColorAttachment.Format = VK_FORMAT_R8G8B8A8_SRGB;
		outputColorAttachment.Sample = true;
		outputColorAttachment.IntialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		outputColorAttachment.FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		framebufferSpecification.Attachments.push_back(outputColorAttachment);

		m_Output = std::make_shared<VulkanFramebuffer>(framebufferSpecification);
	}

	{
		m_TerrainGenerator = std::make_shared<TerrainGenerator>(1024 * 4, 1024 * 4);
	}


	{
		TerrainSpecification terrainSpec;
		terrainSpec.HeightMap = m_TerrainGenerator->getHeightMap();
		terrainSpec.CompositionMap = m_TerrainGenerator->getCompositionMap();
		terrainSpec.NormalMap = m_TerrainGenerator->getNormalMap();

		{
			std::vector<std::string> filepaths = {
				"Resources/Textures/Terrain/grass_d2.png",
				"Resources/Textures/Terrain/slope_d.png",
				"Resources/Textures/Terrain/mountain_d.png",
				"Resources/Textures/Terrain/snow_d.png",
			};

			TextureSpecification texSpec{};
			texSpec.CreateSampler = true;
			texSpec.GenerateMips = true;
			texSpec.Channles = 4;
			texSpec.LayerCount = (uint32_t)filepaths.size();
			texSpec.Filepath = filepaths;
			terrainSpec.TerrainTextures = std::make_shared<VulkanTexture>(texSpec);
		}
		{
			std::vector<std::string> filepaths = {
				"Resources/Textures/Terrain/grass_n2.png",
				"Resources/Textures/Terrain/slope_n.png",
				"Resources/Textures/Terrain/mountain_n.png",
				"Resources/Textures/Terrain/snow_n.png",
			};

			TextureSpecification texSpec{};
			texSpec.CreateSampler = true;
			texSpec.GenerateMips = true;
			texSpec.Channles = 4;
			texSpec.LayerCount = (uint32_t)filepaths.size();
			texSpec.Filepath = filepaths;
			terrainSpec.NormalTextures = std::make_shared<VulkanTexture>(texSpec);
		}

		terrainSpec.Info.TerrainSize = 1024 * 4;
		terrainSpec.Info.LODCount = 3;
		terrainSpec.Filepath = { "2kmterrain.tc", "2kmterrain.tb" };

		m_Terrain = std::make_unique<TerrainData>(terrainSpec);
		m_Terrain->setHeightMultiplier(3300.0f);

		m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

		TerrainGUI = std::make_shared<TerrainGenerationGUI>(m_TerrainGenerator, m_Terrain, 300, ImVec2(1290.0f, 10.0f));
	}

	createFinalPass();

	ClipmapTerrainSpecification clipmapSpec;
	clipmapSpec.ClipmapSize = 1024;

	m_TerrainClipmap = std::make_shared<TerrainClipmap>(clipmapSpec, m_Terrain);
	glm::vec2 camPos = { cam.getPosition().x, cam.getPosition().z };
	m_TerrainClipmap->hardLoad(camPos);

	const std::shared_ptr<VulkanImage> clipmap = m_TerrainClipmap->getMap();

	for (uint32_t lod = 0; lod < m_Terrain->getInfo().LODCount; lod++)
	{
		ImageViewSpecification imvSpec;
		imvSpec.Image = clipmap->getVkImage();
		imvSpec.Aspect = clipmap->getSpecification().Aspect;
		imvSpec.Format = clipmap->getSpecification().Format;
		imvSpec.Layer = lod;
		imvSpec.Mip = 0;

		m_MapViews.push_back(std::make_shared<VulkanImageView>(imvSpec));

		m_MapDescriptors.push_back(ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
			m_MapViews.back()->getImageView(), VK_IMAGE_LAYOUT_GENERAL));
	}
	m_ClipmapLOD = std::make_shared<ClipmapLOD>(m_Terrain->getSpecification(), m_TerrainClipmap);

	createtess();

}

void VulkanApp::onUpdate()
{
	glm::vec3 camVelocity{ 0.0f };
	glm::vec2 rotation{ 0.0f };
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_W))
		camVelocity.z -= 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_S))
		camVelocity.z += 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_D))
		camVelocity.x += 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_A))
		camVelocity.x -= 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_E))
		camVelocity.y += 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_Q))
		camVelocity.y -= 1.0f * Time::deltaTime;

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_UP))
		rotation.x += 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_DOWN))
		rotation.x -= 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_RIGHT))
		rotation.y += 1.0f * Time::deltaTime;
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_LEFT))
		rotation.y -= 1.0f * Time::deltaTime;

	if (camVelocity != glm::vec3(0.0f, 0.0f, 0.0f))
	{
		if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_SPACE))
			cam.Move(glm::normalize(camVelocity) * 5.0f);
		else
			cam.Move(glm::normalize(camVelocity) * 0.10f);
	}

	if (rotation != glm::vec2(0.0f, 0.0f))
		cam.Rotate(rotation);

	cam.updateMatrices();

	glm::vec2 camPos = { cam.getPosition().x, cam.getPosition().z };
	m_TerrainClipmap->pushLoadTasks(camPos);

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	{
		CommandBuffer->Begin();
		VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();
		m_TerrainGenerator->Generate(CommandBuffer);

		if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_3))
			m_TerrainGenerator->runHydraulicErosion(CommandBuffer);

		m_TerrainClipmap->updateClipmaps(commandBuffer);

		VulkanComputePipeline::imageMemoryBarrier(commandBuffer, m_TerrainClipmap->getMap(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, 1, 3);

		uint32_t chunksToR = m_ClipmapLOD->Generate(m_TerrainClipmap->getLastValidCameraPosition());

		static bool a = true;

		//if (a)
		{
			a = false;
			uint32_t lod = 0;
			VulkanRenderer::dispatchCompute(commandBuffer, verticalErrorPass, { 1, 1, 64 * 64 }, sizeof(uint32_t), &lod);
			uint32_t lod1 = 1;
			VulkanRenderer::dispatchCompute(commandBuffer, verticalErrorPass, { 1, 1, 64 * 64 }, sizeof(uint32_t), &lod1);
			uint32_t lod2 = 2;
			VulkanRenderer::dispatchCompute(commandBuffer, verticalErrorPass, { 1, 1, 64 * 64 }, sizeof(uint32_t), &lod2);
		}

		VulkanComputePipeline::imageMemoryBarrier(commandBuffer, m_VerticalErrorMap, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, 1, 3);

		static glm::vec4 thre(0.025f, 0.085f, 0.168f, 0.0f);
		threshold->setDataCPU(&thre, sizeof(glm::vec4));

		{
			VulkanRenderer::beginRenderPass(commandBuffer, m_TerrainRenderPass);
			VulkanRenderer::preparePipeline(commandBuffer, m_TerrainRenderPass);

			CameraRenderMatrices matrices = cam.getRenderMatrices();
			vkCmdPushConstants(commandBuffer, m_TerrainRenderPass.Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0,
				sizeof(CameraRenderMatrices), &matrices);

			VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdDraw(commandBuffer, 4, chunksToR * 64, 0, 0);

			VulkanRenderer::endRenderPass(commandBuffer);
		}

		// Present, fullscreen quad
		{
			CommandBuffer->beginQuery("PresentPass");

			VulkanRenderer::beginSwapchainRenderPass(commandBuffer, m_FinalPass);
			VulkanRenderer::preparePipeline(commandBuffer, m_FinalPass);

			vkCmdDraw(commandBuffer, 6, 1, 0, 0);

			CommandBuffer->beginQuery("Imgui");
			beginImGuiFrame();

			static ProfilerManager manager({480.0f, 10.0f }, 560.0f);
			{
				manager.addProfiler("GPUProfiler", 100);
				manager["GPUProfiler"]->pushValue("TotalGPU", CommandBuffer->getCommandBufferTime(), 0xffff00ff);
				manager["GPUProfiler"]->pushValue("PresentPass", CommandBuffer->getTime("PresentPass"), 0xff0000ff);
				manager["GPUProfiler"]->pushValue("Imgui", CommandBuffer->getTime("Imgui"), 0xff00ffff);
			}

			manager.Render();

			TerrainGUI->Render();

			m_Terrain->setHeightMultiplier(100.0f);

			ImGui::Begin("Clipmaps");
			for (uint32_t lod = 0; lod < 3; lod++)
			{
				ImGui::Image(m_MapDescriptors[lod], ImVec2{ 128, 128 });

				if (lod % 3 != 0 || lod == 0)
					ImGui::SameLine();
			}
			ImGui::End();

			ImGui::Begin("a");

			ImGui::DragFloat("thx", &thre.x, 0.0001);
			ImGui::DragFloat("thy", &thre.y, 0.0001);
			ImGui::DragFloat("thz", &thre.z, 0.0001);
			ImGui::End();



			endImGuiFrame();
			CommandBuffer->endQuery("Imgui");

			VulkanRenderer::endRenderPass(commandBuffer);
			CommandBuffer->endQuery("PresentPass");
		}

		VkClearColorValue colorQuad = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		CommandBuffer->End();
		CommandBuffer->Submit();
	}
	//if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_O))
	//	VirtualTerrainSerializer::Serialize(m_TerrainGenerator->getHeightMap(), "2kmterrain.tb",
	//		"2kmterrain.tc", 128);
}

void VulkanApp::onResize()
{
	TerrainGUI->Position.x = getWidth() - 300.0f - 10.0f;

	m_Output->Resize(getWidth(), getHeight());

	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
		DescriptorSet->bindInput(0, 0, 0, m_Sampler);
		DescriptorSet->bindInput(0, 1, 0, m_Output->getImage(0));
		DescriptorSet->Create();
		m_FinalPass.DescriptorSet = DescriptorSet;
	}
}

void VulkanApp::onDestroy()
{
	m_TerrainGenerator.reset();
}

void VulkanApp::postFrame()
{
	CommandBuffer->queryResults();
}

void VulkanApp::createFinalPass()
{
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("FinalShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "FullscreenPass_vert.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Texture_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
		DescriptorSet->bindInput(0, 0, 0, m_Sampler);
		DescriptorSet->bindInput(0, 1, 0, m_Output->getImage(0));
		DescriptorSet->Create();
		m_FinalPass.DescriptorSet = DescriptorSet;
	}
	{
		PipelineSpecification spec{};
		spec.Framebuffer = nullptr;
		spec.Shader = ShaderManager::getShader("FinalShader");
		spec.vertexBufferLayout = VulkanVertexBufferLayout{};
		m_FinalPass.Pipeline = std::make_shared<VulkanPipeline>(spec);
	}
}

void VulkanApp::createtess()
{
	VulkanBufferProperties terrainInfoProperties;
	terrainInfoProperties.Size = ((uint32_t)sizeof(glm::vec4));
	terrainInfoProperties.Type = BufferType::STORAGE_BUFFER;
	terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

	threshold = std::make_shared<VulkanBuffer>(terrainInfoProperties);

	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("errormap");
		mainShader->addShaderStage(ShaderStage::COMPUTE, "Terrain/ClipmapTessellation/CreateVerticalErrorMap_comp.glsl");
		mainShader->createDescriptorSetLayouts();

		{
			VulkanImageSpecification normalSpecification;
			normalSpecification.Width = 64;
			normalSpecification.Height = 64;
			normalSpecification.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
			normalSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			normalSpecification.LayerCount = 3;
			normalSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			m_VerticalErrorMap = std::make_shared<VulkanImage>(normalSpecification);
			m_VerticalErrorMap->Create();
		}

		verticalErrorPass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("errormap"));
		verticalErrorPass.DescriptorSet->bindInput(0, 0, 0, m_TerrainClipmap->getMap());
		verticalErrorPass.DescriptorSet->bindInput(0, 1, 0, m_VerticalErrorMap);
		verticalErrorPass.DescriptorSet->bindInput(1, 0, 0, m_ClipmapLOD->LODMarginsBufferSet);
		verticalErrorPass.DescriptorSet->Create();

		verticalErrorPass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("errormap"), sizeof(uint32_t));

	}

	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("CLipamptess");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Terrain/ClipmapTessellation/Terrain_vert.glsl");
		mainShader->addShaderStage(ShaderStage::TESSELLATION_CONTROL, "Terrain/ClipmapTessellation/Terrain_tesc.glsl");
		mainShader->addShaderStage(ShaderStage::TESSELLATION_EVALUATION, "Terrain/ClipmapTessellation/Terrain_tese.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Terrain/Terrain_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("CLipamptess"));
		DescriptorSet->bindInput(0, 0, 0, m_ClipmapLOD->chunksToRender);
		DescriptorSet->bindInput(1, 0, 0, m_VerticalErrorMap);
		DescriptorSet->bindInput(1, 1, 0, threshold);
		DescriptorSet->bindInput(1, 2, 0, m_ClipmapLOD->LODMarginsBufferSet);
		DescriptorSet->bindInput(2, 0, 0, m_TerrainClipmap->getMap());
		DescriptorSet->Create();
		m_TerrainRenderPass.DescriptorSet = DescriptorSet;
	}

	PipelineSpecification spec{};
	spec.Framebuffer = m_Output;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = true;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader("CLipamptess");
	spec.vertexBufferLayout = VulkanVertexBufferLayout{ VertexType::INT_2 };
	spec.depthCompareFunction = DepthCompare::LESS;
	spec.Topology = PrimitiveTopology::PATCHES;
	spec.tessellationControlPoints = 4;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass.Pipeline = m_TerrainPipeline;

	std::vector<glm::ivec2> vertices;

	vertices.push_back({ 0, 0 });
	vertices.push_back({ 0, 16 });
	vertices.push_back({ 16, 16 });
	vertices.push_back({ 16, 0 });

	VulkanBufferProperties vertexBufferProperties;
	vertexBufferProperties.Size = (uint32_t)(sizeof(glm::ivec2) * (uint32_t)vertices.size());
	vertexBufferProperties.Type = BufferType::VERTEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	vertexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	m_VertexBuffer = std::make_shared<VulkanBuffer>(vertices.data(), vertexBufferProperties);
}
