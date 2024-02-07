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
	glm::mat4 view;
	glm::mat4 proj;
};

struct Vertex
{
	glm::vec2 Position;
	glm::vec2 texCoord2;
};

#define CHUNK_SIZE 128
struct TerrainChunk
{
	uint32_t xOffset = 0;
	uint32_t yOffset = 0;
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
	std::shared_ptr<VulkanUniformBufferSet> m_OffsetBuffer;
	std::shared_ptr<VulkanUniformBufferSet> m_lodMapBufferSet;

	std::shared_ptr<VulkanRenderPass> m_GeometryPass;
	std::shared_ptr<VulkanRenderPass> m_FinalPass;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	std::vector<TerrainChunk> m_Chunks;

	uint8_t lodLevels[128 * 128];

	uint32_t index = 0;
	std::shared_ptr<VulkanBuffer> m_IndexBuffer;
	uint32_t index1 = 0;
	std::shared_ptr<VulkanBuffer> m_IndexBuffer1;
	uint32_t index2 = 0;
	std::shared_ptr<VulkanBuffer> m_IndexBuffer2;
	uint32_t index3 = 0;
	std::shared_ptr<VulkanBuffer> m_IndexBuffer3;

	std::shared_ptr<VulkanBuffer> m_FullscreenVertexBuffer;
	std::shared_ptr<VulkanBuffer> m_FullscreenIndexBuffer;

	std::shared_ptr<VulkanTexture> m_TextureImage = nullptr;
	std::shared_ptr<VulkanTexture> m_TextureImage2 = nullptr;
	std::shared_ptr<VulkanTexture> m_TerrainTextureArray = nullptr;

	std::shared_ptr<VulkanPipeline> terrainWireframePipeline;
	std::shared_ptr<VulkanPipeline> terrainPipeline;

	Camera cam{45.0f, 1600.0f / 900.0f, 0.1f, 10000.0f};

	bool Wireframe = false;
};