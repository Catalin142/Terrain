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

	m_TextureImage = std::make_shared<VulkanTexture>("Resources/Img/world1.png", false);
	m_TextureImage2 = std::make_shared<VulkanTexture>("Resources/Img/image4.png", 4, false); 
	m_Grass = std::make_shared<VulkanTexture>("Resources/Img/grass.png", 4, true);
	m_Slope = std::make_shared<VulkanTexture>("Resources/Img/slope.png", 4, true);
	m_Rock = std::make_shared<VulkanTexture>("Resources/Img/rock.png", 4, true);

	{																						
		loadModel("Resources/model/viking_room/viking_room.obj");

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
	{
		if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_SPACE))
			cam.Move(glm::normalize(camVelocity) * 5.75f);
		else
			cam.Move(glm::normalize(camVelocity) * 0.75f);
	}

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

		for (auto& chunk : m_Chunks)
		{
			VkBuffer vertexBuffers[] = { chunk.VertexBuffer->getBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			float distance = glm::distance(cam.getPosition(), glm::vec3(chunk.xOffset, 0.0f, chunk.yOffset));

			uint32_t indicesSize = 0;

			if (distance < 150.0f)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				indicesSize = index;
			}

			if (distance >= 150.0f && distance < 200.0f)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer1->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				indicesSize = index1;
			}

			if (distance >= 200.0f && distance < 250.0f)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer2->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				indicesSize = index2;
			}
			
			if (distance >= 250.0f)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer3->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				indicesSize = index3;
			}

			vkCmdDrawIndexed(commandBuffer, uint32_t(indicesSize), 1, 0, 0, 0);
		}


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
		DescriptorSet->bindInput(1, 0, m_TextureImage);
		DescriptorSet->bindInput(1, 1, m_TextureImage2);
		DescriptorSet->bindInput(2, 0, m_Grass);
		DescriptorSet->bindInput(2, 1, m_Slope);
		DescriptorSet->bindInput(2, 2, m_Rock);
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
			VertexType::FLOAT_2,
			VertexType::FLOAT_2,
			});

		PipelineSpecification spec;
		spec.Framebuffer = Framebuffer;
		spec.depthTest = true;
		spec.depthWrite = true;
		spec.Wireframe = false;
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
	{
		uint32_t gridSize = 513;
		for (uint32_t xOffset = 0; xOffset < gridSize / CHUNK_SIZE; xOffset++)
			for (uint32_t yOffset = 0; yOffset < gridSize / CHUNK_SIZE; yOffset++)
			{
				vertices.clear();
				TerrainChunk& chunk = m_Chunks.emplace_back();

				for (int x = xOffset * CHUNK_SIZE; x <= (xOffset + 1) * CHUNK_SIZE; x++)
					for (int y = yOffset * CHUNK_SIZE; y <= (yOffset + 1) * CHUNK_SIZE; y++)
					{
						glm::vec2 UV;
						UV.x = glm::clamp((float)x / (float)gridSize, 0.0f, 1.0f);
						UV.y = glm::clamp((float)y / (float)gridSize, 0.0f, 1.0f);
						glm::vec2 UV2;
						UV2.x = (float)(x - xOffset * CHUNK_SIZE) / (float)CHUNK_SIZE;
						UV2.y = (float)(y - yOffset * CHUNK_SIZE) / (float)CHUNK_SIZE;
						vertices.push_back(Vertex{ glm::vec3(x * 1.0f, 0.0f, y * 1.0f), UV, UV2 });
					}

				chunk.VertexBuffer = std::make_shared<VulkanBuffer>(vertices.data(), (uint32_t)(sizeof(vertices[0]) * (uint32_t)vertices.size()),
					BufferType::VERTEX, BufferUsage::STATIC);
				chunk.xOffset = xOffset * CHUNK_SIZE;
				chunk.yOffset = yOffset * CHUNK_SIZE;
			}

		{
			indices.clear();
			uint32_t lod = 1;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod + lod);

					indices.push_back(index);
					indices.push_back(index + vertCount * lod + lod);
					indices.push_back(index + vertCount * lod);
				}
			}

			index = indices.size();
			m_IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		} 
		
		{
			indices.clear();
			uint32_t lod = 2;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod + lod);

					indices.push_back(index);
					indices.push_back(index + vertCount * lod + lod);
					indices.push_back(index + vertCount * lod);
				}
			}

			index1 = indices.size();
			m_IndexBuffer1 = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		}

		{
			indices.clear();
			uint32_t lod = 4;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod + lod);

					indices.push_back(index);
					indices.push_back(index + vertCount * lod + lod);
					indices.push_back(index + vertCount * lod);
				}
			}

			index2 = indices.size();
			m_IndexBuffer2 = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		}

		{
			indices.clear();
			uint32_t lod = 8;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod + lod);

					indices.push_back(index);
					indices.push_back(index + vertCount * lod + lod);
					indices.push_back(index + vertCount * lod);
				}
			}

			index3 = indices.size();
			m_IndexBuffer3 = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		}
	}
}
