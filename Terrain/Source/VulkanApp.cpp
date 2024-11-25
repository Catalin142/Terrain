#include "VulkanApp.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>

#include "Core/Instrumentor.h"

#include "Graphics/Vulkan/VulkanUtils.h"
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
#include "Terrain/Techniques/DistanceLOD.h"
#include "Terrain/VirtualMap/DynamicVirtualTerrainDeserializer.h"
#include "Terrain/VirtualMap/VirtualTerrainSerializer.h"
#include "Terrain/VirtualMap/VMUtils.h"
#include <backends/imgui_impl_vulkan.h>

#include "GUI/ProfilerGUI.h"

#include <future>
#include "System/Disk.h"

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

		m_Terrain = std::make_shared<Terrain>(terrainSpec);
		m_Terrain->setHeightMultiplier(3300.0f);

		m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

		TerrainGUI = std::make_shared<TerrainGenerationGUI>(m_TerrainGenerator, m_Terrain, 300, ImVec2(1290.0f, 10.0f));
	}

	{
		TerrainQuadTreeProperties terrainQuadProps;
		terrainQuadProps.Size = { 1024, 1024 };
		terrainQuadProps.MaxLod = 4;
		terrainQuadProps.LodDistance = 32;
		terrainQuadTree = new TerrainQuadTree(terrainQuadProps);
	}

	VirtualTerrainSerializer::Init();

	createFinalPass();

	VirtualTerrainMapSpecification spec{};
	spec.Filepaths[VirtualTextureType::HEIGHT].Data = "heightData.tc";
	spec.Filepaths[VirtualTextureType::HEIGHT].Table = "heightTable.tb";
	spec.Format = VK_FORMAT_R16_SFLOAT;
	spec.PhysicalTextureSize = 1024;
	VirtualMap = std::make_shared<TerrainVirtualMap>(spec);

	VirtualTerrainSerializer::Deserialize(VirtualMap, VirtualTextureType::HEIGHT);


	m_Terrain->tvm = VirtualMap;

	m_TerrainRenderer = std::make_shared<TerrainRenderer>(m_Output, m_Terrain);

	DynamicVirtualTerrainDeserializer::Get()->Initialize(VirtualMap);
	m_HeightMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		VirtualMap->m_PhysicalTexture->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);
}

static bool CheckCollision(glm::vec2 onePos, int oneSize, glm::vec2 twoPos, int twoSize)
{
	bool collisionX = onePos.x + oneSize >= twoPos.x &&
		twoPos.x + twoSize >= onePos.x;
	bool collisionY = onePos.y + oneSize >= twoPos.y &&
		twoPos.y + twoSize >= onePos.y;
	return collisionX && collisionY;
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

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_1))
		m_TerrainRenderer->setWireframe(true);

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_2))
		m_TerrainRenderer->setWireframe(false);

	terrainQuadTree->insertPlayer({ cam.getPosition().x, cam.getPosition().z });

	//std::vector<TerrainChunk> chunksToRender = m_Terrain->getChunksToRender(glm::vec3(0.0f, 0.0f, 0.0f));
	std::vector<TerrainChunk> chunksToRender;

	// TODO: Store lods in terrain
	// world size 1024
	std::vector<TerrainChunk> rings;
	int32_t initialRingSize = 1024 / 8;
	glm::vec2 posRing1 = glm::vec2(cam.getPosition().x - initialRingSize / 2 - 1, cam.getPosition().z - initialRingSize - 1);
	initialRingSize *= 2;																			
	glm::vec2 posRing2 = glm::vec2(cam.getPosition().x - initialRingSize / 2 - 1, cam.getPosition().z - initialRingSize - 1);
	initialRingSize *= 2;																			
	glm::vec2 posRing3 = glm::vec2(cam.getPosition().x - initialRingSize / 2 - 1, cam.getPosition().z - initialRingSize - 1);
	initialRingSize *= 2;																			
	glm::vec2 posRing4 = glm::vec2(cam.getPosition().x - initialRingSize / 2 - 1, cam.getPosition().z - initialRingSize - 1);

	for (const auto& [offset, mip1] : VirtualMap->m_ChunkProperties)
	{
		// get position
		uint32_t xu, yu;
		unpackOffset(VirtualMap->m_ChunkPosition[offset], xu, yu);


		uint32_t mip = VirtualMap->m_ChunkMip[offset];

		// mip1: 128, mip2: 256, mip3: 512, mip4: 1024
		uint32_t size = 128 * (1 << mip);

		xu *= size;
		yu *= size;

		switch (mip)
		{
		case 0:
			if (CheckCollision(posRing1, 129, { xu, yu }, size))
				chunksToRender.push_back(TerrainChunk{ { xu, yu }, size, 1 });
			break;

		case 1:
			if (CheckCollision(posRing2, 257, { xu, yu }, size))
				chunksToRender.push_back(TerrainChunk{ { xu, yu }, size, 2 });
			break;

		case 2:
			if (CheckCollision(posRing3, 513, { xu, yu }, size))
				chunksToRender.push_back(TerrainChunk{ { xu, yu }, size, 4 });
			break;

		case 3:
			if (CheckCollision(posRing4, 1025, { xu, yu }, size))
				chunksToRender.push_back(TerrainChunk{ { xu, yu }, size, 8 });
			break;
		}

	}

	m_Terrain->tvm->updateVirtualMap(chunksToRender);
	DynamicVirtualTerrainDeserializer::Get()->Refresh();

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	{
		CommandBuffer->Begin();
		VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();
		m_TerrainGenerator->Generate(CommandBuffer);


		if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_3))
			m_TerrainGenerator->runHydraulicErosion(CommandBuffer);

		/*CameraCompParams compParams;

		compParams.forward = glm::vec2(cam.Forward().x, cam.Forward().z);
		compParams.position = glm::vec2(cam.getPosition().x / 1024.0f * 100.0f, cam.getPosition().z / 1024.0f * 100.0f);
		compParams.fov = 45.0f;
		compParams.right = glm::vec2(cam.Right().x, cam.Right().z);*/

		glm::vec2 cameraPosition = { (cam.getPosition().x), (cam.getPosition().z) };

		static int xHH = 0, yHH = 0, mipHH = 0;

		// Geometry pass
		{
			CommandBuffer->beginQuery("GeometryPass");
			m_TerrainRenderer->setRenderCommandBuffer(CommandBuffer);
			m_TerrainRenderer->Render(cam);
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

			static ProfilerManager manager({ 10.0f, 10.0f }, 400.0f);
			{
				manager.addProfiler("GPUProfiler", 100);
				manager["GPUProfiler"]->addProfileValue("TotalGPU", CommandBuffer->getCommandBufferTime(), 0xffff00ff);
				manager["GPUProfiler"]->addProfileValue("GeometryPass", CommandBuffer->getTime("GeometryPass"), 0xffffffff);
				manager["GPUProfiler"]->addProfileValue("PresentPass", CommandBuffer->getTime("PresentPass"), 0xff0000ff);
				manager["GPUProfiler"]->addProfileValue("Imgui", CommandBuffer->getTime("Imgui"), 0xff00ffff);
			}
			{
				manager.addProfiler("CPUProfiler", 100);
				manager["CPUProfiler"]->addProfileValue("Total Time", Instrumentor::Get().getTime("_TotalTime"), 0xffffffff);
				manager["CPUProfiler"]->addProfileValue("Update", Instrumentor::Get().getTime("_Update"), 0xffff00ff);
			}

			manager.Render();

			TerrainGUI->Render();

			m_Terrain->setHeightMultiplier(100.0f);

			ImGui::Begin("VirtualHeightMapDebug");

			ImGui::Image(m_HeightMapDescriptor, ImVec2{ 512, 512 });

			ImGui::DragInt("x", &xHH);
			ImGui::DragInt("y", &yHH);
			ImGui::DragInt("mip", &mipHH);

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
		VirtualTerrainSerializer::Serialize(m_TerrainGenerator->getHeightMap(), VirtualMap->getSpecification(), VirtualTextureType::HEIGHT);
	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_P))
	{
		VirtualTerrainSerializer::Deserialize(VirtualMap, VirtualTextureType::HEIGHT);
	}
}

void VulkanApp::onResize()
{
	TerrainGUI->Position.x = getWidth() - 300.0f - 10.0f;

	m_Output->Resize(getWidth(), getHeight());

	m_TerrainRenderer->setTargetFramebuffer(m_Output);

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
	DynamicVirtualTerrainDeserializer::Get()->Shutdown();
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