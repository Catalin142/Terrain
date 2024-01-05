#pragma once
#include <memory>
#include <vector>
#include <optional>
#include <array>

#include "Graphics/VulkanDevice.h"
#include "Graphics/VulkanSwapchain.h"
#include "Core/Window.h"
#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include "Graphics/VulkanUniformBuffer.h"
#include "Graphics/VulkanTexture.h"
#include "Graphics/VulkanBuffer.h"
#include "Graphics/VulkanVertexBufferLayout.h"
#include "Graphics/VulkanRenderCommandBuffer.h"
#include "Graphics/VulkanRenderer.h"
#include "Graphics/VulkanImgui.h"

#define MAX_FRAMES_IN_FLIGHT 2

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec2 texCoord;
	glm::vec3 color;

	bool operator==(const Vertex& other) const {
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};

class VulkanApp
{
public:
	VulkanApp();
	~VulkanApp();

	void Run();

private:
	void initVulkan();

	void mainLoop();

	void createGeometryPass();
	void createFinalPass();

	void drawFrame();
	void updateUniformBuffer(uint32_t currentImage);

	void recreateSwapChain();

	void loadModel(const std::string& filepath);

private:
	std::shared_ptr<Window> window;
	std::shared_ptr<VulkanDevice> m_Device;
	std::shared_ptr<VulkanSwapchain> m_SwapChain;

	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	std::shared_ptr<VulkanUniformBufferSet> m_UniformBufferSet;

	std::shared_ptr<VulkanRenderPass> m_GeometryPass;
	std::shared_ptr<VulkanRenderPass> m_FinalPass;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::shared_ptr<VulkanBuffer> m_VertexBuffer;
	std::shared_ptr<VulkanBuffer> m_IndexBuffer;

	std::shared_ptr<VulkanBuffer> m_FullscreenVertexBuffer;
	std::shared_ptr<VulkanBuffer> m_FullscreenIndexBuffer;

	std::shared_ptr<VulkanTexture> m_TextureImage = nullptr;

	VulkanImgui imguiLayer;
};