#include "VulkanApp.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>

#include "Core/Instrumentor.h"

#include "Graphics/VulkanUtils.h"
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

#include "stb_image/std_image.h"
#include "Graphics/VulkanShader.h"

VulkanApp::VulkanApp(const std::string& title, uint32_t width, uint32_t height) : Application(title, width, height)
{ }

void VulkanApp::onCreate()
{
	CommandBuffer = std::make_shared<VulkanRenderCommandBuffer>(true);
	
	{
		float FullscreenVertices[] = {
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f,
			-1.0f,  1.0f, 0.0f, 1.0f
		};

		uint32_t FullscreenIndices[] = {
			0, 1, 2,
			0, 2, 3
		};

		m_FullscreenVertexBuffer = std::make_shared<VulkanBuffer>(FullscreenVertices, (uint32_t)(sizeof(float) * 16),
			BufferType::VERTEX, BufferUsage::STATIC);

		m_FullscreenIndexBuffer = std::make_shared<VulkanBuffer>(FullscreenIndices, (uint32_t)(sizeof(uint32_t) * 6),
			BufferType::INDEX, BufferUsage::STATIC);
	}

	{
		FramebufferSpecification fbSpec;
		fbSpec.Width = 1600;
		fbSpec.Height = 900;
		fbSpec.Samples = 1;
		fbSpec.DepthAttachment.Format = VK_FORMAT_D32_SFLOAT;
		fbSpec.DepthAttachment.Blend = false;
		FramebufferAttachment color1{};
		color1.Format = VK_FORMAT_R8G8B8A8_SRGB;
		color1.Sample = true;
		fbSpec.Attachments.push_back(color1);

		std::shared_ptr<VulkanFramebuffer> Framebuffer;
		m_Output = std::make_shared<VulkanFramebuffer>(fbSpec);
	}
	{
		TerrainSpecification terrainSpec;
		terrainSpec.HeightMap = "Resources/Img/heightmap2.png";
		terrainSpec.CompositionMap = "Resources/Img/image4.png";

		terrainSpec.TerrainTextures.push_back("Resources/Img/grass.png");
		terrainSpec.TerrainTextures.push_back("Resources/Img/slope.png");
		terrainSpec.TerrainTextures.push_back("Resources/Img/rock.png");

		m_Terrain = std::make_shared<Terrain>(terrainSpec);

		m_TerrainRenderer = std::make_shared<TerrainRenderer>(m_Output);
	}

	createFinalPass();
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
			cam.Move(glm::normalize(camVelocity) * 5.75f);
		else
			cam.Move(glm::normalize(camVelocity) * 0.75f);
	}

	if (rotation != glm::vec2(0.0f, 0.0f))
		cam.Rotate(rotation);
	cam.updateMatrices();

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_1))
		m_TerrainRenderer->setWireframe(true);

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_2))
		m_TerrainRenderer->setWireframe(false);

	CommandBuffer->Begin();
	VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();

	// Geometry pass
	{
		CommandBuffer->beginQuery("GeometryPass");
		m_TerrainRenderer->setRenderCommandBuffer(CommandBuffer);
		m_TerrainRenderer->renderTerrain(cam, m_Terrain);
		CommandBuffer->endQuery("GeometryPass");
	}

	// Present, fullscreen quad
	{
		CommandBuffer->beginQuery("PresentPass");

		VulkanRenderer::beginSwapchainRenderPass(CommandBuffer, m_FinalPass);

		VkBuffer vertexBuffers[] = { m_FullscreenVertexBuffer->getBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, m_FullscreenIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

		CommandBuffer->beginQuery("Imgui");
		beginImGuiFrame();
		ImGui::Begin("GPU Time");
		ImGui::Text("Total time = %f ms", CommandBuffer->getCommandBufferTime());
		ImGui::Text("Geometry Pass = %f ms", CommandBuffer->getTime("GeometryPass"));
		ImGui::Text("Present Pass = %f ms", CommandBuffer->getTime("PresentPass"));
		ImGui::Text("Imgui = %f ms", CommandBuffer->getTime("Imgui"));
		ImGui::End();

		ImGui::Begin("CPU Time");
		ImGui::Text("Update = %f ms", Instrumentor::Get().getTime("_Update"));
		ImGui::Text("Total Time = %f ms", Instrumentor::Get().getTime("_TotalTime"));
		ImGui::End();

		endImGuiFrame();
		CommandBuffer->endQuery("Imgui");

		VulkanRenderer::endRenderPass(CommandBuffer);
		CommandBuffer->endQuery("PresentPass");
	}

	CommandBuffer->End();
	CommandBuffer->Submit();
}

void VulkanApp::onResize()
{
	m_TerrainRenderer->onResize(getWidth(), getHeight());

	std::shared_ptr<VulkanDescriptorSet> DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
	DescriptorSet->bindInput(0, 0, m_Output->getImage(0));
	DescriptorSet->Create();
	m_FinalPass->setDescriptorSet(DescriptorSet);
}

void VulkanApp::onDestroy()
{
}

void VulkanApp::postFrame()
{
	CommandBuffer->queryResults();
}


void VulkanApp::createFinalPass()
{
	m_FinalPass = std::make_shared<VulkanRenderPass>();
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("FinalShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "fullscreenPass_vert.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "texture_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
		DescriptorSet->bindInput(0, 0, m_Output->getImage(0));
		DescriptorSet->Create();
		m_FinalPass->setDescriptorSet(DescriptorSet);
	}
	{
		VulkanVertexBufferLayout VBOLayout = VulkanVertexBufferLayout({
			VertexType::FLOAT_2,
			VertexType::FLOAT_2,
			});

		PipelineSpecification spec;
		spec.Framebuffer = nullptr;
		spec.Shader = ShaderManager::getShader("FinalShader");
		spec.vertexBufferLayout = VBOLayout;
		m_FinalPass->setPipeline(std::make_shared<VulkanPipeline>(spec));
	}
}