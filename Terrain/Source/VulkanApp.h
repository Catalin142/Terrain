#pragma once
#include <memory>
#include <vector>
#include <optional>
#include <array>

#include "Core/Application.h"
#include "Graphics/Vulkan/VulkanTexture.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Graphics/Vulkan/VulkanVertexBufferLayout.h"
#include "Graphics/Vulkan/VulkanRenderCommandBuffer.h"
#include "Graphics/Vulkan/VulkanComputePipeline.h"
#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanImgui.h"

#include "Renderer/QuadTreeTerrainRenderer.h"

#include "Terrain/Terrain.h"
#include "Terrain/Generator/TerrainGenerator.h"

#include "Scene/Camera.h"
#include "GUI/TerrainGenerationGUI.h"

#include "Terrain/VirtualMap/TerrainVirtualMap.h"
#include "Renderer/ClipmapTerrainRenderer.h"

#include "Terrain/LODManager.h"

#include "GUI/LODManagerGUI.h"

#define MAX_FRAMES_IN_FLIGHT 2

struct CameraCompParams
{
	glm::vec2 position;
	glm::vec2 forward;
	glm::vec2 right;
	float fov;
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

	void createtess();

private:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	RenderPass m_FinalPass;

	std::shared_ptr<VulkanFramebuffer> m_Output;

	std::unique_ptr<TerrainData> m_Terrain;
	std::shared_ptr<TerrainGenerator> m_TerrainGenerator;

	std::shared_ptr<VulkanSampler> m_Sampler;
	std::shared_ptr<TerrainGenerationGUI> TerrainGUI;

	std::shared_ptr<TerrainVirtualMap> VirtualMap;

	Camera cam{45.0f, 1600.0f / 900.0f, 0.1f, 5000.0f};

	RenderPass m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	std::shared_ptr<VulkanBuffer> m_VertexBuffer;
	uint32_t vCount;

	std::shared_ptr<TerrainClipmap> m_TerrainClipmap;

	std::vector<VkDescriptorSet> m_MapDescriptors;
	std::vector<std::shared_ptr<VulkanImageView>> m_MapViews;

	VulkanComputePass verticalErrorPass;
	std::shared_ptr<VulkanImage> m_VerticalErrorMap;
	std::shared_ptr<ClipmapLOD> m_ClipmapLOD;

	std::shared_ptr<VulkanBuffer> threshold;
};