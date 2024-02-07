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

	{
		TextureSpecification texSpec{};
		texSpec.CreateSampler = true;
		texSpec.Filepath.push_back("Resources/Img/heightmap2.png");
		m_TextureImage = std::make_shared<VulkanTexture>(texSpec);
	}
	{
		TextureSpecification texSpec{};
		texSpec.CreateSampler = true;
		texSpec.Channles = 4;
		texSpec.Filepath.push_back("Resources/Img/image4.png");
		m_TextureImage2 = std::make_shared<VulkanTexture>(texSpec);
	}
	{
		{
			TextureSpecification texSpec{};
			texSpec.CreateSampler = true;
			texSpec.GenerateMips = true;
			texSpec.Channles = 4;
			texSpec.LayerCount = 3;
			texSpec.Filepath.push_back("Resources/Img/grass.png");
			texSpec.Filepath.push_back("Resources/Img/slope.png");
			texSpec.Filepath.push_back("Resources/Img/rock.png");
			m_TerrainTextureArray = std::make_shared<VulkanTexture>(texSpec);
		}
	}

	{
		// need to store chunks offsets
		m_OffsetBuffer = std::make_shared<VulkanUniformBufferSet>(256 * sizeof(glm::vec4), MAX_FRAMES_IN_FLIGHT);
		m_lodMapBufferSet = std::make_shared<VulkanUniformBufferSet>(128 * 128 * sizeof(uint8_t), MAX_FRAMES_IN_FLIGHT);
		loadModel("Resources/model/viking_room/viking_room.obj");
	}
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
		uint32_t lod0Count = 0;
		uint32_t lod1Count = 0;
		uint32_t lod2Count = 0;
		uint32_t lod3Count = 0;

		{
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance < 450.0f)
				{
					lodLevels[currentChunk.xOffset / CHUNK_SIZE, currentChunk.yOffset / CHUNK_SIZE] = 0;
					lod0Count++;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 4.0, 0.0f });
				}
			}
		}
		{

			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 450.0f && distance < 850.0f)
				{
					lodLevels[currentChunk.xOffset / CHUNK_SIZE, currentChunk.yOffset / CHUNK_SIZE] = 1;
					lod1Count++;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 8.0, 0.0f });
				}
			}
		}
		{
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 850.0f && distance < 1250.0f)
				{
					lod2Count++;
					lodLevels[currentChunk.xOffset / CHUNK_SIZE, currentChunk.yOffset / CHUNK_SIZE] = 2;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 16.0, 0.0f });
				}
			}
		}
		{
			for (auto& currentChunk : m_Chunks)
			{
				float distance = glm::distance(cam.getPosition(), glm::vec3(currentChunk.xOffset, 0.0f, currentChunk.yOffset));
				if (distance >= 1250.0f)
				{
					lod3Count++;
					lodLevels[currentChunk.xOffset / CHUNK_SIZE, currentChunk.yOffset / CHUNK_SIZE] = 3;
					offsets.push_back({ currentChunk.xOffset, currentChunk.yOffset, 32.0, 0.0f });
				}
			}
		}

		m_OffsetBuffer->setData(offsets.data(), offsets.size() * sizeof(glm::vec4), VulkanRenderer::getCurrentFrame());
		m_lodMapBufferSet->setData(offsets.data(), offsets.size() * sizeof(glm::vec4), VulkanRenderer::getCurrentFrame());

		uint32_t firstInstance = 0;
		{
			START_SCOPE_PROFILE("Lod0Render");
			if (lod0Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index), lod0Count, 0, 0, firstInstance);
				firstInstance += lod0Count;
			}
		}
		{
			START_SCOPE_PROFILE("Lod1Render");
			if (lod1Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer1->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index1), lod1Count, 0, 0, firstInstance);
				firstInstance += lod1Count;
			}
		}
		{
			START_SCOPE_PROFILE("Lod2Render");
			if (lod2Count)
			{
				vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer2->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(commandBuffer, uint32_t(index2), lod2Count, 0, 0, firstInstance);
				firstInstance += lod2Count;
			}
		}

		{
			START_SCOPE_PROFILE("Lod3Render");
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
		mainShader->addShaderStage(ShaderStage::VERTEX, "simple_vertex.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "simple_fragment.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("GeometryShader"));
		DescriptorSet->bindInput(0, 0, m_UniformBufferSet);
		DescriptorSet->bindInput(0, 1, m_OffsetBuffer);
		DescriptorSet->bindInput(0, 2, m_lodMapBufferSet);
		DescriptorSet->bindInput(1, 0, m_TextureImage);
		DescriptorSet->bindInput(2, 0, m_TextureImage2);
		DescriptorSet->bindInput(2, 1, m_TerrainTextureArray);
		
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
		spec.vertexBufferLayout = VulkanVertexBufferLayout{};
		//spec.vertexBufferLayout = VBOLayout;
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
		mainShader->addShaderStage(ShaderStage::VERTEX, "finalPass_vertex.vert");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "finalPass_fragment.frag");
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

void VulkanApp::updateUniformBuffer(uint32_t currentImage)
{
	VkExtent2D extent = getExtent();

	UniformBufferObject ubo{};
	ubo.view = cam.getView();
	ubo.proj = cam.getProjection();

	m_UniformBufferSet->setData(&ubo, sizeof(ubo), currentImage);
}

void generateIndices(std::vector<uint32_t>& indices, uint32_t lod, uint32_t vertCount)
{
	for (uint32_t x = 0; x < vertCount - 1; x += lod * 2) {
		for (uint32_t y = 0; y < vertCount - 1; y += lod * 2) {
			uint32_t index = x * vertCount + y;
			/*
			c      d      i


			b      e      h


			a      f      g
			*/

			uint32_t a = index;
			uint32_t b = index + lod;
			uint32_t c = index + lod * 2;
			uint32_t d = index + vertCount * lod + lod * 2;
			uint32_t e = index + vertCount * lod + lod;
			uint32_t f = index + vertCount * lod;
			uint32_t g = index + vertCount * lod * 2;
			uint32_t h = index + vertCount * lod * 2 + lod;
			uint32_t i = index + vertCount * lod * 2 + lod * 2;

			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(e);

			indices.push_back(f);
			indices.push_back(a);
			indices.push_back(e);

			indices.push_back(b);
			indices.push_back(c);
			indices.push_back(e);

			indices.push_back(e);
			indices.push_back(c);
			indices.push_back(d);

			indices.push_back(e);
			indices.push_back(d);
			indices.push_back(i);

			indices.push_back(h);
			indices.push_back(e);
			indices.push_back(i);

			indices.push_back(f);
			indices.push_back(e);
			indices.push_back(g);

			indices.push_back(g);
			indices.push_back(e);
			indices.push_back(h);
		}
	}
}

void VulkanApp::loadModel(const std::string& filepath)
{
	{
		uint32_t gridSize = 1024;
		std::vector<glm::vec4> offsets;
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
			generateIndices(indices, lod, vertCount);

			index = indices.size();
			m_IndexBuffer = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		} 
		
		{
			indices.clear();
			uint32_t lod = 8;
			uint32_t vertCount = CHUNK_SIZE + 1;
			generateIndices(indices, lod, vertCount);

			index1 = indices.size();
			m_IndexBuffer1 = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		}

		{
			indices.clear();
			uint32_t lod = 16;
			uint32_t vertCount = CHUNK_SIZE + 1;
			generateIndices(indices, lod, vertCount);

			index2 = indices.size();
			m_IndexBuffer2 = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		}

		{
			indices.clear();
			uint32_t lod = 32;
			uint32_t vertCount = CHUNK_SIZE + 1;
			generateIndices(indices, lod, vertCount);

			index3 = indices.size();
			m_IndexBuffer3 = std::make_shared<VulkanBuffer>(indices.data(), (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size()),
				BufferType::INDEX, BufferUsage::STATIC);
		}
	}
}
