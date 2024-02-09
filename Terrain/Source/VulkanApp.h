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
#include "Graphics/VulkanRenderer.h"
#include "Graphics/VulkanImgui.h"

#include "Renderer/TerrainRenderer.h"

#include "Terrain/Terrain.h"

#include "Scene/Camera.h"

#define MAX_FRAMES_IN_FLIGHT 2

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
	std::shared_ptr<VulkanRenderPass> m_FinalPass;

	std::shared_ptr<VulkanFramebuffer> m_Output;

	std::shared_ptr<VulkanBuffer> m_FullscreenVertexBuffer;
	std::shared_ptr<VulkanBuffer> m_FullscreenIndexBuffer;

	std::shared_ptr<Terrain> m_Terrain;
	std::shared_ptr<TerrainRenderer> m_TerrainRenderer;
	
	Camera cam{45.0f, 1600.0f / 900.0f, 0.1f, 10000.0f};

	bool Wireframe = false;
};