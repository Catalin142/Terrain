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
#include "Terrain/VirtualMap/VirtualMapUtils.h"
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
		m_TerrainGenerator = std::make_shared<TerrainGenerator>(1024, 1024);
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

		terrainSpec.Info.TerrainSize = { 1024.0, 1024.0 };
		terrainSpec.Info.LodCount = 4;

		m_Terrain = std::make_shared<Terrain>(terrainSpec);
		m_Terrain->setHeightMultiplier(3300.0f);

		m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

		TerrainGUI = std::make_shared<TerrainGenerationGUI>(m_TerrainGenerator, m_Terrain, 300, ImVec2(1290.0f, 10.0f));
	}

	//VirtualTerrainSerializer::Init();

	createFinalPass();

	/*m_HeightMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		VirtualMap->getPhysicalTexture()->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);*/

	VirtualTerrainMapSpecification VirtualMapSpecification{};
	VirtualMapSpecification.Filepath.Data = "heightData.tc";
	VirtualMapSpecification.Filepath.Table = "heightTable.tb";
	VirtualMapSpecification.Format = VK_FORMAT_R16_SFLOAT;
	VirtualMapSpecification.PhysicalTextureSize = 1024 * 2;
	VirtualMapSpecification.RingSizes = std::array<uint32_t, MAX_LOD>{6, 9, 13, 6, 0, 0};

	QuadTreeTerrainRendererSpecification quadTreeRendererSpecification;
	quadTreeRendererSpecification.targetFramebuffer = m_Output;
	quadTreeRendererSpecification.Terrain = m_Terrain;
	quadTreeRendererSpecification.VirtualMapSpecification = VirtualMapSpecification;

	m_Renderer = std::make_shared<QuadTreeTerrainRenderer>(quadTreeRendererSpecification);
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
			cam.Move(glm::normalize(camVelocity) * 2.0f);
		else
			cam.Move(glm::normalize(camVelocity) * 0.10f);
	}

	if (rotation != glm::vec2(0.0f, 0.0f))
		cam.Rotate(rotation);
	cam.updateMatrices();

	m_Renderer->updateVirtualMap(cam.getPosition());

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_1))
		m_Renderer->setWireframe(true);
	
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_2))
		m_Renderer->setWireframe(false);

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	{
		CommandBuffer->Begin();
		VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();
		m_TerrainGenerator->Generate(CommandBuffer);

		if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_3))
			m_TerrainGenerator->runHydraulicErosion(CommandBuffer);

		m_Renderer->commandBuffer = CommandBuffer;
		m_Renderer->Precompute();

		// Geometry pass
		{
			CommandBuffer->beginQuery("GeometryPass");

			m_Renderer->Render(cam);
			
			CommandBuffer->endQuery("GeometryPass");
		}

		// Present, fullscreen quad
		{
			CommandBuffer->beginQuery("PresentPass");

			VulkanRenderer::beginSwapchainRenderPass(CommandBuffer, m_FinalPass);
			VulkanRenderer::preparePipeline(CommandBuffer, m_FinalPass);

			vkCmdDraw(commandBuffer, 6, 1, 0, 0);

			CommandBuffer->beginQuery("Imgui");
			beginImGuiFrame();

			static ProfilerManager manager({ 10.0f, 10.0f }, 640.0f);
			{
				manager.addProfiler("GPUProfiler", 100);
				manager["GPUProfiler"]->pushValue("TotalGPU", CommandBuffer->getCommandBufferTime(), 0xffff00ff);
				manager["GPUProfiler"]->pushValue("GeometryPass", CommandBuffer->getTime("GeometryPass"), 0xffffffff);
				manager["GPUProfiler"]->pushValue("PresentPass", CommandBuffer->getTime("PresentPass"), 0xff0000ff);
				manager["GPUProfiler"]->pushValue(QuadTreeRendererMetrics::RENDER_TERRAIN, CommandBuffer->getTime(QuadTreeRendererMetrics::RENDER_TERRAIN), 0xff0000ff);
				manager["GPUProfiler"]->pushValue("Imgui", CommandBuffer->getTime("Imgui"), 0xff00ffff);
			}
			{
				manager.addProfiler("CPUProfiler", 100);
				manager["CPUProfiler"]->pushValue("Total Time", Instrumentor::Get().getTime("_TotalTime"), 0xffffffff);
				manager["CPUProfiler"]->pushValue("Update", Instrumentor::Get().getTime("_Update"), 0xffff00ff);
			}

			{
				manager.addProfiler("Terrain", 100);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES, Instrumentor::Get().getTime(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES), 0xffffffff);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::GPU_UPDATE_VIRTUAL_MAP, CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_UPDATE_VIRTUAL_MAP), 0xffffff00);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::GPU_UPDATE_STATUS_TEXTURE, CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_UPDATE_STATUS_TEXTURE), 0xffff00ff);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::GPU_UPDATE_INDIRECTION_TEXTURE, CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_UPDATE_INDIRECTION_TEXTURE), 0xff00ffff);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::GPU_GENERATE_QUAD_TREE, CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_GENERATE_QUAD_TREE), 0xffff0000);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP, CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP), 0xff0000ff);
				manager["Terrain"]->pushValue(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND, CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND), 0xff00ff00);
			
			}

			manager.Render();

			TerrainGUI->Render();

			m_Terrain->setHeightMultiplier(100.0f);

			ImGui::Begin("VirtualHeightMapDebug");
			//ImGui::Image(m_HeightMapDescriptor, ImVec2{ 520, 520 });
			ImGui::End();

			endImGuiFrame();
			CommandBuffer->endQuery("Imgui");

			VulkanRenderer::endRenderPass(CommandBuffer);
			CommandBuffer->endQuery("PresentPass");
		}

		VkClearColorValue colorQuad = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		CommandBuffer->End();
		CommandBuffer->Submit();
	}
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_O))
		VirtualTerrainSerializer::Serialize(m_TerrainGenerator->getHeightMap(), VirtualMap->getSpecification().Filepath.Table, 
			VirtualMap->getSpecification().Filepath.Data, 128);
	//if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_P))
	//{
	//	VirtualTerrainSerializer::Deserialize(VirtualMap, VirtualTextureType::HEIGHT);
	//}
}

void VulkanApp::onResize()
{
	TerrainGUI->Position.x = getWidth() - 300.0f - 10.0f;

	m_Output->Resize(getWidth(), getHeight());

	//m_TerrainRenderer->setTargetFramebuffer(m_Output);

	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
		DescriptorSet->bindInput(0, 0, 0, m_Sampler);
		DescriptorSet->bindInput(0, 1, 0, m_Output->getImage(0));
		DescriptorSet->Create();
		m_FinalPass->DescriptorSet = DescriptorSet;
	}
}

void VulkanApp::onDestroy()
{
	m_TerrainGenerator.reset();
}

void VulkanApp::postFrame()
{
	CommandBuffer->queryResults();

	//if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_U))
	//{
	//	thread = new std::thread([&]() {
	//		ShaderManager::getShader("_NormalCompute")->getShaderStage(ShaderStage::COMPUTE)->Recompile();
	//		vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());
	//		m_TerrainGenerator->m_NormalComputePass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_NormalCompute"));
	//		});
	//	//thread->join();
	//	presed = true;
	//}
}

void VulkanApp::createFinalPass()
{
	m_FinalPass = std::make_shared<RenderPass>();
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
		m_FinalPass->DescriptorSet = DescriptorSet;
	}
	{
		PipelineSpecification spec{};
		spec.Framebuffer = nullptr;
		spec.Shader = ShaderManager::getShader("FinalShader");
		spec.vertexBufferLayout = VulkanVertexBufferLayout{};
		m_FinalPass->Pipeline = std::make_shared<VulkanPipeline>(spec);
	}
}
