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

#include "Scene/Camera.h"

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
	void createGeometryPass();
	void createFinalPass();

	void updateUniformBuffer(uint32_t currentImage);

	void loadModel(const std::string& filepath);

private:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	std::shared_ptr<VulkanUniformBufferSet> m_UniformBufferSet;

	std::shared_ptr<VulkanRenderPass> m_GeometryPass;
	std::shared_ptr<VulkanRenderPass> m_FinalPass;

	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::shared_ptr<VulkanBuffer> m_VertexBuffer;
	std::shared_ptr<VulkanBuffer> m_IndexBuffer;

	std::shared_ptr<VulkanBuffer> m_FullscreenVertexBuffer;
	std::shared_ptr<VulkanBuffer> m_FullscreenIndexBuffer;

	std::shared_ptr<VulkanTexture> m_TextureImage = nullptr;

	Camera cam{45.0f, 1600.0f / 900.0f, 0.1f, 1000.0f};
};