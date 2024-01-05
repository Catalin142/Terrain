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

VulkanApp::VulkanApp()
{
	window = std::make_shared<Window>(1600, 900, "Vulkan");
}

VulkanApp::~VulkanApp()
{
	imguiLayer.Destroy();
	ShaderManager::Clear();
}

void VulkanApp::Run()
{
	initVulkan();
	imguiLayer.Initialize(window);
	mainLoop();
}

void VulkanApp::initVulkan()
{
	InstanceProperties props = getDefaultInstanceProperties();
	m_Device = std::make_shared<VulkanDevice>(window, props);

	m_SwapChain = std::make_shared<VulkanSwapchain>();
	m_SwapChain->Initialize();
	m_SwapChain->Create(1600, 900);

	VulkanRenderer::Initialize(m_SwapChain);

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

void VulkanApp::mainLoop()
{
	float begTime = glfwGetTime();
	float endTime = begTime;
	float dt = 0.0f;
	while (window->isOpened())
	{
		begTime = glfwGetTime();
		dt = (begTime - endTime);
		endTime = begTime;

		window->Update();
		drawFrame();
	}
	VulkanRenderer::Destroy();
	vkDeviceWaitIdle(m_Device->getLogicalDevice());
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
		DescriptorSet->bindInput(0, 1, m_TextureImage);
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
		FramebufferAttachment color2{};
		color2.Format = VK_FORMAT_R8G8B8A8_SRGB;
		fbSpec.Attachments.push_back(color1);
		fbSpec.Attachments.push_back(color2);

		std::shared_ptr<VulkanFramebuffer> Framebuffer;
		Framebuffer = std::make_shared<VulkanFramebuffer>(fbSpec);

		VulkanVertexBufferLayout VBOLayout = VulkanVertexBufferLayout({
			VertexType::FLOAT_3,
			VertexType::FLOAT_2,
			VertexType::FLOAT_3,
			});

		PipelineSpecification spec;
		spec.Framebuffer = Framebuffer;
		spec.depthTest = true;
		spec.depthWrite = true;
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

void VulkanApp::drawFrame()
{
	if (window->Resized)
	{
		recreateSwapChain();
		m_GeometryPass->Resize(window->getWidth(), window->getHeight());

		std::shared_ptr<VulkanDescriptorSet> DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("FinalShader"));
		DescriptorSet->bindInput(0, 0, m_GeometryPass->getOutput(0));
		DescriptorSet->Create();
		m_FinalPass->setDescriptorSet(DescriptorSet);

		window->Resized = false;
	}

	m_SwapChain->beginFrame();

	CommandBuffer->Begin();
	VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();

	// Geometry pass
	{
		CommandBuffer->beginQuery("GeometryPass");

		VulkanRenderer::beginRenderPass(CommandBuffer, m_GeometryPass); 
		
		updateUniformBuffer(m_SwapChain->getCurrentFrame());
		
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
		imguiLayer.beginFrame();
		ImGui::Begin("Render time"); 
		ImGui::Text("Total time = %f ms", CommandBuffer->getCommandBufferTime());
		ImGui::Text("Geometry Pass = %f ms", CommandBuffer->getTime("GeometryPass"));
		ImGui::Text("Present Pass = %f ms", CommandBuffer->getTime("PresentPass"));
		ImGui::Text("Imgui = %f ms", CommandBuffer->getTime("Imgui"));
		ImGui::End();
		imguiLayer.endFrame();
		CommandBuffer->endQuery("Imgui");
		
		VulkanRenderer::endRenderPass(CommandBuffer);
		CommandBuffer->endQuery("PresentPass");
	}

	CommandBuffer->End();
	CommandBuffer->Submit();
	
	uint32_t frame = m_SwapChain->getCurrentFrame();

	m_SwapChain->endFrame();
	CommandBuffer->queryResults();
}

void VulkanApp::updateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VkExtent2D extent = m_SwapChain->getExtent();

	UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), 0.05f * time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
	ubo.proj[1][1] *= -1;

	m_UniformBufferSet->setData(&ubo, sizeof(ubo), currentImage);
}

void VulkanApp::recreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window->getHandle(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window->getHandle(), &width, &height);
		glfwWaitEvents();
	}

	m_SwapChain->onResize(width, height);
}

void VulkanApp::loadModel(const std::string& filepath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };
			vertices.push_back(vertex);

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}
}
