#include "VulkanApp.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>

#include "Graphics/VulkanUtils.h"
#include "tiny_obj_loader.h"
#include <imgui.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

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

	m_TextureImage = std::make_shared<VulkanTexture>("Resources/model/viking_room/viking_room.png", false);

	{
		loadModel("Resources/model/viking_room/viking_room.obj");

		m_VertexBuffer = std::make_shared<VulkanBuffer>(vertices.data(), (uint32_t)(sizeof(vertices[0]) * (uint32_t)vertices.size()),
			BufferType::VERTEX, BufferUsage::STATIC);

		m_IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
			BufferType::INDEX, BufferUsage::STATIC);
	}
	{
		float FullscreenVertices[] = {
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f
		};

		uint32_t FullscreenIndices[] = {
			0, 1, 2,
			0, 2, 3
		};

		m_FullscreenVertexBuffer = std::make_shared<VulkanBuffer>(FullscreenVertices, (uint32_t)(sizeof(float) * 20),
			BufferType::VERTEX, BufferUsage::STATIC);

		m_FullscreenIndexBuffer = std::make_shared<VulkanBuffer>(FullscreenIndices, (uint32_t)(sizeof(uint32_t) * 6),
			BufferType::INDEX, BufferUsage::STATIC);
	}

	m_UniformBufferSet = std::make_shared<VulkanUniformBufferSet>(uint32_t(sizeof(UniformBufferObject)), MAX_FRAMES_IN_FLIGHT);

	createGeometryPass();
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
		cam.Move(glm::normalize(camVelocity) * 0.25f);

	if (rotation != glm::vec2(0.0f, 0.0f))
	{
		cam.Rotate(rotation);
	}

	CommandBuffer->Begin();
	VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();

	// Geometry pass
	{
		CommandBuffer->beginQuery("GeometryPass");

		VulkanRenderer::beginRenderPass(CommandBuffer, m_GeometryPass);

		updateUniformBuffer(VulkanRenderer::getCurrentFrame());

		VkBuffer vertexBuffers[] = { m_VertexBuffer->getBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandBuffer, uint32_t(indices.size()), 1, 0, 0, 0);

		VulkanRenderer::endRenderPass(CommandBuffer);

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
		ImGui::Begin("Render time");
		ImGui::Text("Total time = %f ms", CommandBuffer->getCommandBufferTime());
		ImGui::Text("Geometry Pass = %f ms", CommandBuffer->getTime("GeometryPass"));
		ImGui::Text("Present Pass = %f ms", CommandBuffer->getTime("PresentPass"));
		ImGui::Text("Imgui = %f ms", CommandBuffer->getTime("Imgui"));
		ImGui::Text("pp = %f ms", CommandBuffer->getTime("Pipeline"));
		ImGui::End();
		endImGuiFrame();
		CommandBuffer->endQuery("Imgui");

		CommandBuffer->beginQuery("Pipeline");
		VulkanRenderer::endRenderPass(CommandBuffer);
		CommandBuffer->endQuery("Pipeline");
		CommandBuffer->endQuery("PresentPass");
	}

	CommandBuffer->End();
	CommandBuffer->Submit();
}

void VulkanApp::onResize()
{
	m_GeometryPass->Resize(getWidth(), getHeight());

	std::shared_ptr<VulkanDescriptorSet> DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
	DescriptorSet->bindInput(0, 0, m_GeometryPass->getOutput(0));
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

void VulkanApp::createGeometryPass()
{
	m_GeometryPass = std::make_shared<VulkanRenderPass>();

	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("GeometryShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Resources/Shaders/simple_vertex.vert");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Resources/Shaders/simple_fragment.frag");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("GeometryShader"));
		DescriptorSet->bindInput(0, 0, m_UniformBufferSet);
		DescriptorSet->Create();
		m_GeometryPass->setDescriptorSet(DescriptorSet);
	}
	{
		FramebufferSpecification fbSpec;
		fbSpec.Width = 1600;
		fbSpec.Height = 900;
		fbSpec.Samples = 1;
		fbSpec.DepthAttachment.Format = VK_FORMAT_D32_SFLOAT;
		FramebufferAttachment color1{};
		color1.Format = VK_FORMAT_R8G8B8A8_SRGB;
		color1.Sample = true;
		fbSpec.Attachments.push_back(color1);

		std::shared_ptr<VulkanFramebuffer> Framebuffer;
		Framebuffer = std::make_shared<VulkanFramebuffer>(fbSpec);

		VulkanVertexBufferLayout VBOLayout = VulkanVertexBufferLayout({
			VertexType::FLOAT_3,
			});

		PipelineSpecification spec;
		spec.Framebuffer = Framebuffer;
		spec.depthTest = true;
		spec.depthWrite = true;
		spec.Wireframe = true;
		spec.Culling = true;
		spec.Shader = ShaderManager::getShader("GeometryShader");
		spec.vertexBufferLayout = VBOLayout;
		spec.depthCompareFunction = DepthCompare::LESS;
		m_GeometryPass->setPipeline(std::make_shared<VulkanPipeline>(spec));
	}
}

void VulkanApp::createFinalPass()
{
	m_FinalPass = std::make_shared<VulkanRenderPass>();
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("FinalShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Resources/Shaders/finalPass_vertex.vert");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Resources/Shaders/finalPass_fragment.frag");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
		DescriptorSet->bindInput(0, 0, m_GeometryPass->getOutput(0));
		DescriptorSet->Create();
		m_FinalPass->setDescriptorSet(DescriptorSet);
	}
	{
		VulkanVertexBufferLayout VBOLayout = VulkanVertexBufferLayout({
			VertexType::FLOAT_3,
			VertexType::FLOAT_2,
			});

		PipelineSpecification spec;
		spec.Framebuffer = nullptr;
		spec.Shader = ShaderManager::getShader("FinalShader");
		spec.vertexBufferLayout = VBOLayout;
		m_FinalPass->setPipeline(std::make_shared<VulkanPipeline>(spec));
	}
}

void VulkanApp::updateUniformBuffer(uint32_t currentImage)
{
	VkExtent2D extent = getExtent();

	UniformBufferObject ubo{};
	ubo.model = glm::mat4(1.0f);
	ubo.view = cam.getView();
	ubo.proj = cam.getProjection();

	m_UniformBufferSet->setData(&ubo, sizeof(ubo), currentImage);
}

void VulkanApp::loadModel(const std::string& filepath)
{
	uint32_t gridSize = 64 * 2;
	for (int x = 0; x < gridSize + 1; ++x)
		for (int y = 0; y < gridSize + 1; ++y)
			vertices.push_back(glm::vec3(x * 0.2f, 0.0f, y * 0.2f));

	uint32_t vertCount = gridSize + 1;
	for (int i = 0; i < vertCount * vertCount - vertCount; ++i)
	{
		if ((i + 1) % vertCount == 0)
		{
			continue;
		}
		indices.push_back(i + 1 + vertCount);
		indices.push_back(i + vertCount);
		indices.push_back(i);
		indices.push_back(i);
		indices.push_back(i + 1);
		indices.push_back(i + 1 + vertCount);
	}
}
