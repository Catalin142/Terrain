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

		terrainSpec.Info.TerrainSize = { 1024.0, 1024.0 };
		terrainSpec.Info.LodCount = 4;

		m_Terrain = std::make_shared<Terrain>(terrainSpec);
		m_Terrain->setHeightMultiplier(3300.0f);

		m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

		TerrainGUI = std::make_shared<TerrainGenerationGUI>(m_TerrainGenerator, m_Terrain, 300, ImVec2(1290.0f, 10.0f));
	}

	//VirtualTerrainSerializer::Init();

	createFinalPass();

	ClipmapSpecification clipmap;
	clipmap.ChunkSize = 128;
	clipmap.TerrainSize = 1024 * 4;
	clipmap.Filepath = TerrainFileLocation{ "compress_clipmapdata.tc", "compress_clipmaptable.tb" };
	clipmap.LODCount = 3;
	clipmap.TextureSize = 1024;

	m_Clipmap = std::make_shared<TerrainClipmap>(clipmap);

	VirtualTerrainSerializer::Deserialize(m_Clipmap);
	createClipmapBuffers();

	{
		{
			ImageViewSpecification imvSpec;
			imvSpec.Image = m_Clipmap->getMap()->getVkImage();
			imvSpec.Aspect = m_Clipmap->getMap()->getSpecification().Aspect;
			imvSpec.Format = m_Clipmap->getMap()->getSpecification().Format;
			imvSpec.Layer = 0;
			imvSpec.Mip = 0;

			imageView = std::make_shared<VulkanImageView>(imvSpec);

			m_HeightMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
				imageView->getImageView(), VK_IMAGE_LAYOUT_GENERAL);
		}

		{
			ImageViewSpecification imvSpec;
			imvSpec.Image = m_Clipmap->getMap()->getVkImage();
			imvSpec.Aspect = m_Clipmap->getMap()->getSpecification().Aspect;
			imvSpec.Format = m_Clipmap->getMap()->getSpecification().Format;
			imvSpec.Layer = 1;
			imvSpec.Mip = 0;

			imageView1 = std::make_shared<VulkanImageView>(imvSpec);

			m_HeightMapDescriptor1 = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
				imageView1->getImageView(), VK_IMAGE_LAYOUT_GENERAL);
		}

		{
			ImageViewSpecification imvSpec;
			imvSpec.Image = m_Clipmap->getMap()->getVkImage();
			imvSpec.Aspect = m_Clipmap->getMap()->getSpecification().Aspect;
			imvSpec.Format = m_Clipmap->getMap()->getSpecification().Format;
			imvSpec.Layer = 2;
			imvSpec.Mip = 0;

			imageView2 = std::make_shared<VulkanImageView>(imvSpec);

			m_HeightMapDescriptor2 = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
				imageView2->getImageView(), VK_IMAGE_LAYOUT_GENERAL);
		}

		/*{
			ImageViewSpecification imvSpec;
			imvSpec.Image = m_Clipmap->getMap()->getVkImage();
			imvSpec.Aspect = m_Clipmap->getMap()->getSpecification().Aspect;
			imvSpec.Format = m_Clipmap->getMap()->getSpecification().Format;
			imvSpec.Layer = 3;
			imvSpec.Mip = 0;

			imageView3 = std::make_shared<VulkanImageView>(imvSpec);

			m_HeightMapDescriptor3 = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
				imageView3->getImageView(), VK_IMAGE_LAYOUT_GENERAL);
		}*/

		glm::vec2 camPosss = { cam.getPosition().x, cam.getPosition().z };
		m_Clipmap->hardLoad(camPosss);
	}
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
			cam.Move(glm::normalize(camVelocity) * 10.0f);
		else
			cam.Move(glm::normalize(camVelocity) * 0.10f);
	}

	if (rotation != glm::vec2(0.0f, 0.0f))
		cam.Rotate(rotation);
	cam.updateMatrices();

	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();
	renderClipmap(cam);

	{
		CommandBuffer->Begin();
		VkCommandBuffer commandBuffer = CommandBuffer->getCurrentCommandBuffer();
		m_TerrainGenerator->Generate(CommandBuffer);

		if (glfwGetKey(getWindow()->getHandle(), GLFW_KEY_3))
			m_TerrainGenerator->runHydraulicErosion(CommandBuffer);

		glm::vec2 camPosss = { cam.getPosition().x, cam.getPosition().z };

		Instrumentor::Get().beginTimer(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES);
		m_Clipmap->Refresh(commandBuffer, camPosss);
		Instrumentor::Get().endTimer(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES);

		// Geometry pass
		{
			//CommandBuffer->beginQuery("GeometryPass");
			renderTerrain(cam);
			//CommandBuffer->endQuery("GeometryPass");
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
			static float maxTime = 0;
			maxTime = glm::max(Instrumentor::Get().getTime(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES), maxTime);
			ImGui::Text(std::to_string(maxTime).c_str());

			
			ImGui::Image(m_HeightMapDescriptor,  ImVec2{ 256, 256 });
			ImGui::SameLine();
			ImGui::Image(m_HeightMapDescriptor1, ImVec2{ 256, 256 });
			ImGui::Image(m_HeightMapDescriptor2, ImVec2{ 256, 256 });
			//ImGui::SameLine();
			//ImGui::Image(m_HeightMapDescriptor3, ImVec2{ 256, 256 });
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
		VirtualTerrainSerializer::SerializeClipmap(m_TerrainGenerator->getHeightMap(), "compress_clipmaptable.tb",
			"compress_clipmapdata.tc", 128);
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


struct TerrainChunkWithPos
{
	glm::vec2 position;
	uint32_t lod;
	uint32_t paddin;
};

void VulkanApp::createClipmapBuffers()
{
	uint32_t vertCount = 128 + 1;
	std::vector<uint32_t> indices = TerrainChunk::generateIndices(1, vertCount);

	idxCount = (uint32_t)indices.size();

	VulkanBufferProperties indexBufferProperties;
	indexBufferProperties.Size = (uint32_t)(sizeof(indices[0]) * (uint32_t)indices.size());
	indexBufferProperties.Type = BufferType::INDEX_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	indexBufferProperties.Usage = BufferMemoryUsage::BUFFER_ONLY_GPU;

	indexBuffer = std::make_shared<VulkanBuffer>(indices.data(), indexBufferProperties);

	VulkanBufferProperties resultProperties;
	resultProperties.Size = 1024 * (uint32_t)sizeof(TerrainChunk);
	resultProperties.Type = BufferType::STORAGE_BUFFER;
	resultProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

	ChunksToRender = std::make_shared<VulkanBufferSet>(VulkanRenderer::getFramesInFlight(), resultProperties);


	{
		VulkanBufferProperties camInfoProperties;
		camInfoProperties.Size = ((uint32_t)sizeof(glm::vec2));
		camInfoProperties.Type = BufferType::UNIFORM_BUFFER;
		camInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		CamInfo = std::make_shared<VulkanBuffer>(camInfoProperties);
	}

	{
		VulkanBufferProperties terrainInfoProperties;
		terrainInfoProperties.Size = ((uint32_t)sizeof(TerrainInfo));
		terrainInfoProperties.Type = BufferType::UNIFORM_BUFFER;
		terrainInfoProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		TerrainInfdo = std::make_shared<VulkanBuffer>(terrainInfoProperties);
	}

	m_TerrainRenderPass = std::make_shared<RenderPass>();
	{
		std::shared_ptr<VulkanShader>& mainShader = ShaderManager::createShader("CircleShader");
		mainShader->addShaderStage(ShaderStage::VERTEX, "Terrain/TerrainCircle_vert.glsl");
		mainShader->addShaderStage(ShaderStage::FRAGMENT, "Terrain/Terrain_frag.glsl");
		mainShader->createDescriptorSetLayouts();
	}
	{
		std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
		DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("CircleShader"));
		DescriptorSet->bindInput(0, 0, 0, ChunksToRender);
		DescriptorSet->bindInput(0, 1, 0, TerrainInfdo);
		DescriptorSet->bindInput(0, 2, 0, CamInfo);
		DescriptorSet->bindInput(1, 0, 0, m_Clipmap->getMap());
		DescriptorSet->bindInput(1, 3, 0, m_Terrain->getCompositionMap());
		DescriptorSet->bindInput(1, 4, 0, m_Terrain->getNormalMap());
		DescriptorSet->bindInput(2, 0, 0, m_Terrain->getTerrainTextures());
		DescriptorSet->bindInput(2, 1, 0, m_Terrain->getNormalTextures());

		DescriptorSet->Create();
		m_TerrainRenderPass->DescriptorSet = DescriptorSet;
	}

	PipelineSpecification spec{};
	spec.Framebuffer = m_Output;
	spec.depthTest = true;
	spec.depthWrite = true;
	spec.Wireframe = false;
	spec.Culling = true;
	spec.Shader = ShaderManager::getShader("CircleShader");
	spec.vertexBufferLayout = VulkanVertexBufferLayout{};
	spec.depthCompareFunction = DepthCompare::LESS;

	spec.pushConstants.push_back({ sizeof(CameraRenderMatrices), VK_SHADER_STAGE_VERTEX_BIT });

	m_TerrainPipeline = std::make_shared<VulkanPipeline>(spec);

	m_TerrainRenderPass->Pipeline = m_TerrainPipeline;
}

void VulkanApp::renderClipmap(Camera camera)
{
}

void VulkanApp::renderTerrain(Camera camera)
{
	uint32_t m_CurrentFrame = VulkanRenderer::getCurrentFrame();

	TerrainInfo terrainIfo = m_Terrain->getInfo();
	TerrainInfdo->setDataCPU(&terrainIfo, sizeof(TerrainInfo));
	
	glm::vec3 camPos = camera.getPosition();
	CamInfo->setDataCPU(&camPos, sizeof(glm::vec2));

	auto commandBuffer = CommandBuffer->getCurrentCommandBuffer();

	VulkanRenderer::beginRenderPass(CommandBuffer, m_TerrainRenderPass);
	VulkanRenderer::preparePipeline(CommandBuffer, m_TerrainRenderPass);

	CameraRenderMatrices matrices = camera.getRenderMatrices();
	vkCmdPushConstants(CommandBuffer->getCurrentCommandBuffer(), m_TerrainRenderPass->Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(CameraRenderMatrices), &matrices);

	std::vector<TerrainChunk> chunks;

	int32_t prevminY;
	int32_t prevmaxY;
	int32_t prevminX;
	int32_t prevmaxX;

	for (int32_t lod = 0; lod < 3; lod++)
	{
		TerrainChunk tc;
		tc.Lod = lod;

		int32_t size = 128 << lod;

		int32_t chunkSize = 128 * (1 << lod);

		glm::ivec2 snapedPosition;
		glm::vec3 newCamPos = camPos;
		newCamPos.x = glm::max(newCamPos.x, chunkSize * 4.0f);
		newCamPos.z = glm::max(newCamPos.z, chunkSize * 4.0f);
		newCamPos.x = glm::min(newCamPos.x, 1024.0f * 4.0f - chunkSize * 4.0f);
		newCamPos.z = glm::min(newCamPos.z, 1024.0f * 4.0f - chunkSize * 4.0f);

		snapedPosition.x = int32_t(newCamPos.x) / chunkSize;
		snapedPosition.y = int32_t(newCamPos.z) / chunkSize;

		int32_t minY = glm::max(int32_t(snapedPosition.y) - 8 / 2, 0);
		int32_t maxY = glm::min(int32_t(snapedPosition.y) + 8 / 2, 1024 * 4 / chunkSize);

		int32_t minX = glm::max(int32_t(snapedPosition.x) - 8 / 2, 0);
		int32_t maxX = glm::min(int32_t(snapedPosition.x) + 8 / 2, 1024 * 4 / chunkSize);

		if (snapedPosition.x % 2 != 0)
		{
			minX -= 1;
			maxX -= 1;
		}

		if (snapedPosition.y % 2 != 0)
		{
			minY -= 1;
			maxY -= 1;
		}

		for (int32_t y = minY; y < maxY; y++)
			for (int32_t x = minX; x < maxX; x++)
			{
				tc.Offset = packOffset(x, y);

				if (lod != 0)
				{
					bool add = true;
					
					uint32_t marginminY	= prevminY / 2;
					uint32_t marginmaxY	= prevmaxY / 2;
					uint32_t marginminX	= prevminX / 2;
					uint32_t marginmaxX	= prevmaxX / 2;

					if (x >= marginminX && x < marginmaxX && y >= marginminY && y < marginmaxY)
						add = false;

					if (add)
						chunks.push_back(tc);
				}
				else
					chunks.push_back(tc);
			}

		prevminY = minY;
		prevmaxY = maxY;
		prevminX = minX;
		prevmaxX = maxX;
	}

	ChunksToRender->getCurrentFrameBuffer()->setDataCPU(chunks.data(), chunks.size() * sizeof(TerrainChunk));

	vkCmdBindIndexBuffer(CommandBuffer->getCurrentCommandBuffer(), indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(CommandBuffer->getCurrentCommandBuffer(), idxCount, chunks.size(), 0, 0, 0);

	VulkanRenderer::endRenderPass(CommandBuffer);
}
