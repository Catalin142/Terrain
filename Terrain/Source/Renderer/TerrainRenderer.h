#pragma once

#include "Terrain/Terrain.h"

#include "Graphics/Vulkan/VulkanPipeline.h"
#include "Graphics/Vulkan/VulkanPass.h"
#include "Graphics/Vulkan/VulkanImage.h"
#include "Graphics/Vulkan/VulkanUniformBuffer.h"
#include "Graphics/Vulkan/VulkanRenderCommandBuffer.h"

#include "Scene/Camera.h"

#include <memory>

#define DEBUG_TERRAIN_NORMALS 0

struct TerrainChunkIndexBuffer
{
	std::shared_ptr<VulkanBuffer> IndexBuffer;
	uint32_t IndicesCount = 0;
};

class TerrainRenderer
{
public:
	TerrainRenderer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer, const std::shared_ptr<Terrain>& terrain);
	~TerrainRenderer() = default;

	void Render(const Camera& camera);

	void setRenderCommandBuffer(const std::shared_ptr<VulkanRenderCommandBuffer>& renderCommandBuffer) 
	{ m_RenderCommandBuffer = renderCommandBuffer; }
	
	void setTargetFramebuffer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer);

	const std::shared_ptr<VulkanImage> getOutput() { return m_TerrainRenderPass->Pipeline->getTargetFramebuffer()->getImage(0); }

	void setWireframe(bool wireframe);
	std::shared_ptr<VulkanUniformBufferSet> m_TerrainChunksSet;

private:
	void initializeBuffers();

	void initializeRenderPass();

	void createRenderPass();
	void createPipeline();
	void renderTerrain(const Camera& camera);

	const TerrainChunkIndexBuffer& getLOD(uint8_t lod);

private:
	std::shared_ptr<Terrain> m_Terrain;

	std::shared_ptr<VulkanSampler> m_TerrainSampler;
	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;
	std::shared_ptr<VulkanRenderCommandBuffer> m_RenderCommandBuffer;

	std::shared_ptr<RenderPass> m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	// LOD Map
	std::shared_ptr<VulkanImage> m_LODMap;
	std::shared_ptr<VulkanComputePass> m_LODMapComputePass;
	//

	std::shared_ptr<VulkanUniformBuffer> m_TerrainInfo;

	bool m_InWireframe = false;

	std::shared_ptr<VulkanTexture> m_NoiseMap;

	std::unordered_map<uint8_t, TerrainChunkIndexBuffer> m_LODIndexBuffer;

private: // DEBUG

#if DEBUG_TERRAIN_NORMALS == 1 
	void createNormalDebugRenderPass();
	std::shared_ptr<RenderPass> m_NormalsDebugRenderPass;
#endif
};