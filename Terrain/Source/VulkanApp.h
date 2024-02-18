#pragma once
#include <memory>
#include <vector>
#include <optional>
#include <array>

#include "Core/Application.h"
#include "Graphics/VulkanUniformBuffer.h"
#include "Graphics/VulkanTexture.h"
#include "Graphics/VulkanBuffer.h"
#include "Graphics/VulkanVertexBufferLayout.h"
#include "Graphics/VulkanRenderCommandBuffer.h"
#include "Graphics/VulkanComputePipeline.h"
#include "Graphics/VulkanRenderer.h"
#include "Graphics/VulkanImgui.h"

#include "Renderer/TerrainRenderer.h"

#include "Terrain/Terrain.h"
#include "Terrain/Generator/TerrainGenerator.h"

#include "Scene/Camera.h"
#include "GUI/TerrainGenerationGUI.h"

#include <thread>

#define MAX_FRAMES_IN_FLIGHT 2

struct noiseParams
{
	int32_t octaves = 1;
	float amplitude = 0.5f;
	float frequency = 0.0f;
	float gain = 0.5f;
	float lacunarity = 2.0f;
	float padding[2];
};

class VulkanApp : public Application
{
public:
	VulkanApp(const std::string& title, uint32_t width, uint32_t height);
	~VulkanApp() = default;

	void onCreate() override;
	void onUpdate() override;
	void onResize() override;
	void onDestroy() override;
	void postFrame() override;

private:
	void createFinalPass();

private:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	std::shared_ptr<RenderPass> m_FinalPass;

	std::shared_ptr<VulkanFramebuffer> m_Output;

	std::shared_ptr<Terrain> m_Terrain;
	std::shared_ptr<TerrainRenderer> m_TerrainRenderer;
	std::shared_ptr<TerrainGenerator> m_TerrainGenerator;

	std::shared_ptr<VulkanSampler> m_Sampler;
	std::shared_ptr<TerrainGenerationGUI> TerrainGUI;

	Camera cam{45.0f, 1600.0f / 900.0f, 0.1f, 1000000000.0f};
	bool Wireframe = false;

	std::thread* thread;
	bool presed = false;
};