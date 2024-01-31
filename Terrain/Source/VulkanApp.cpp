#include "VulkanApp.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>

#include "Core/Benchmark.h"

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

	TextureSpecification texSpec{};
	texSpec.createSampler = true;
	{
		m_TextureImage = std::make_shared<VulkanTexture>("Resources/Img/heightmap2.png", texSpec);
	}
	{
		texSpec.Channles = 4;
		m_TextureImage2 = std::make_shared<VulkanTexture>("Resources/Img/image4.png", texSpec);
	}
	{
		texSpec.generateMips = true;
		texSpec.createSampler = false;
		m_Grass = std::make_shared<VulkanTexture>("Resources/Img/grass.png", texSpec);
		m_Slope = std::make_shared<VulkanTexture>("Resources/Img/slope.png", texSpec);
		m_Rock = std::make_shared<VulkanTexture>("Resources/Img/rock.png", texSpec);

		SamplerSpecification spec{};
		spec.Mips = m_Rock->getInfo().mipCount;
		m_Sampler = std::make_shared<VulkanSampler>(spec);
	}

	{
		// need to store chunks offsets
		m_OffsetBuffer = std::make_shared<VulkanUniformBufferSet>(256 * sizeof(glm::vec4), MAX_FRAMES_IN_FLIGHT);
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

	if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_TAB))
	{
		Wireframe = !Wireframe;
		if (Wireframe)
			m_GeometryPass->setPipeline(terrainWireframePipeline);
		else
			m_GeometryPass->setPipeline(terrainPipeline);

	}

	CommandBuffer->Begin();
	VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();

	// Geometry pass
	{
		START_SCOPE_PROFILE("GeometryPass");
		CommandBuffer->beginQuery("GeometryPass");

		VulkanRenderer::beginRenderPass(CommandBuffer, m_GeometryPass);

		updateUniformBuffer(VulkanRenderer::getCurrentFrame());

		//VkBuffer vertexBuffers[] = { VertexBuffer->getBuffer() };
		//VkDeviceSize offsets[] = { 0 };
		//vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		std::vector<glm::vec4> offsets;
		{

			uint32_t lod0Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance < 150.0f)
				{
					lod0Count++;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 0.0, 0.0f });
				}
			}
		}
		{

			uint32_t lod1Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 150.0f && distance < 550.0f)
				{
					lod1Count++;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 0.0, 0.0f });
				}
			}
		}
		{

			uint32_t lod2Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 550.0f && distance < 750.0f)
				{
					lod2Count++;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 0.0, 0.0f });
				}
			}
		}

		{

			uint32_t lod3Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 750.0f)
				{
					lod3Count++;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 0.0, 0.0f });
				}
			}
		}

		m_OffsetBuffer->setData(offsets.data(), offsets.size() * sizeof(glm::vec4), VulkanRenderer::getCurrentFrame());

		uint32_t firstInstance = 0;
		{
			START_SCOPE_PROFILE("Lod0Render");

			uint32_t lod0Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance < 150.0f)
				{
					lod0Count++;
				}
			}
			if (lod0Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index), lod0Count, 0, 0, firstInstance);
				firstInstance += lod0Count;
			}
		}
		{
			START_SCOPE_PROFILE("Lod1Render");

			uint32_t lod1Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 150.0f && distance < 550.0f)
				{
					lod1Count++;
				}
			}

			if (lod1Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer1->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index1), lod1Count, 0, 0, firstInstance);
				firstInstance += lod1Count;
			}
		}
		{
			START_SCOPE_PROFILE("Lod2Render");

			uint32_t lod2Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 550.0f && distance < 750.0f)
				{
					lod2Count++;
				}
			}

			if (lod2Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer2->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index2), lod2Count, 0, 0, firstInstance);
				firstInstance += lod2Count;
			}
		}

		{
			START_SCOPE_PROFILE("Lod3Render");

			uint32_t lod3Count = 0;
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 750.0f)
				{
					lod3Count++;
				}
			}

			if (lod3Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer3->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index3), lod3Count, 0, 0, firstInstance);
				firstInstance += lod3Count;
			}
		}
		VulkanRenderer::endRenderPass(CommandBuffer);

		CommandBuffer->endQuery("GeometryPass");
	}

	// Present, fullscreen quad
	{
		START_SCOPE_PROFILE("PresentPass");
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
		ImGui::Text("Lod 0 = %f ms", Instrumentor::Get().getTime("Lod0Render"));
		ImGui::Text("Lod 1 = %f ms", Instrumentor::Get().getTime("Lod1Render"));
		ImGui::Text("Lod 2 = %f ms", Instrumentor::Get().getTime("Lod2Render"));
		ImGui::Text("Lod 3 = %f ms", Instrumentor::Get().getTime("Lod3Render"));
		ImGui::Text("mainLoop = %f ms", Instrumentor::Get().getTime("mainLoop"));
		ImGui::Text("endFrame = %f ms", Instrumentor::Get().getTime("endFrame"));
		ImGui::Text("Geometry Pass = %f ms", Instrumentor::Get().getTime("GeometryPass"));
		ImGui::Text("Present Pass = %f ms", Instrumentor::Get().getTime("PresentPass"));
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
		mainShader->addShaderStage(ShaderStage::VERTEX, "Resources/Shaders/simple_vertex.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Resources/Shaders/simple_fragment.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("GeometryShader"));
		DescriptorSet->bindInput(0, 0, m_UniformBufferSet);
		DescriptorSet->bindInput(0, 1, m_OffsetBuffer);
		DescriptorSet->bindInput(1, 0, m_TextureImage);
		DescriptorSet->bindInput(2, 0, m_TextureImage2);
		DescriptorSet->bindInput(2, 1, m_Grass);
		DescriptorSet->bindInput(2, 2, m_Slope);
		DescriptorSet->bindInput(2, 3, m_Rock);
		DescriptorSet->bindInput(2, 4, m_Sampler);
		
		DescriptorSet->Create();
		m_GeometryPass->setDescriptorSet(DescriptorSet);
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
		Framebuffer = std::make_shared<VulkanFramebuffer>(fbSpec);

		PipelineSpecification spec;
		spec.Framebuffer = Framebuffer;
		spec.depthTest = true;
		spec.depthWrite = true;
		spec.Wireframe = false;
		spec.Culling = true;
		spec.Shader = ShaderManager::getShader("GeometryShader");
		spec.vertexBufferLayout = VulkanVertexBufferLayout({});
		spec.depthCompareFunction = DepthCompare::LESS;
		terrainPipeline = std::make_shared<VulkanPipeline>(spec);

		spec.Wireframe = true;
		terrainWireframePipeline = std::make_shared<VulkanPipeline>(spec);

		m_GeometryPass->setPipeline(terrainPipeline);
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
	ubo.view = cam.getView();
	ubo.proj = cam.getProjection();

	m_UniformBufferSet->setData(&ubo, sizeof(ubo), currentImage);
}

void VulkanApp::loadModel(const std::string& filepath)
{
	{
		std::vector<glm::vec4> offsets;
		uint32_t gridSize = 1024;
		for (uint32_t xOffset = 0; xOffset < gridSize / CHUNK_SIZE; xOffset++)
			for (uint32_t yOffset = 0; yOffset < gridSize / CHUNK_SIZE; yOffset++)
			{
				vertices.clear();
				TerrainChunk& chunk = m_Chunks.emplace_back();

				chunk.xOffset = xOffset * CHUNK_SIZE;
				chunk.yOffset = yOffset * CHUNK_SIZE;
				offsets.push_back({ chunk.xOffset, chunk.yOffset, 0.0f, 0.0f });
			}

		m_OffsetBuffer->setData(offsets.data(), offsets.size() * sizeof(glm::vec4), VulkanRenderer::getCurrentFrame());

		{
			indices.clear();
			uint32_t lod = 4;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod);

					indices.push_back(index + lod);
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
			uint32_t lod = 8;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod);

					indices.push_back(index + lod);
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
			uint32_t lod = 16;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod);

					indices.push_back(index + lod);
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
			uint32_t lod = 32;
			uint32_t vertCount = CHUNK_SIZE + 1;
			for (int x = 0; x < vertCount - 1; x += lod) {
				for (int y = 0; y < vertCount - 1; y += lod) {
					uint32_t index = x * vertCount + y;
					indices.push_back(index);
					indices.push_back(index + lod);
					indices.push_back(index + vertCount * lod);

					indices.push_back(index + lod);
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
