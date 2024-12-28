#pragma once

#include "Terrain/Terrain.h"

#include "Graphics/Vulkan/VulkanPipeline.h"
#include "Graphics/Vulkan/VulkanPass.h"
#include "Graphics/Vulkan/VulkanImage.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
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

	const std::shared_ptr<VulkanImage> getOutput() { return m_TerrainQuadRenderPass->Pipeline->getTargetFramebuffer()->getImage(0); }

	void setWireframe(bool wireframe);

	inline static std::shared_ptr<VulkanBufferSet> m_TerrainChunksSet;

	inline static std::shared_ptr<VulkanBuffer> m_IndirectDraw;

	// set private
	void initializeRenderPass();
	const TerrainChunkIndexBuffer& getChunkIndexBufferLOD(uint8_t lod);

private:
	void initializeBuffers();


	void createQuadRenderPass();
	void createQuadPipeline();

	void createCircleRenderPass();
	void createCirclePipeline();

	void renderTerrain(const Camera& camera);

private:
	std::shared_ptr<Terrain> m_Terrain;

	std::shared_ptr<VulkanSampler> m_TerrainSampler;
	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;
	std::shared_ptr<VulkanRenderCommandBuffer> m_RenderCommandBuffer;

	std::shared_ptr<RenderPass> m_TerrainQuadRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainQuadPipeline;

	std::shared_ptr<RenderPass> m_TerrainCircleRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainCirclePipeline;

	// LOD Map
	std::shared_ptr<VulkanImage> m_LODMap;
	std::shared_ptr<VulkanComputePass> m_LODMapComputePass;
	//

	std::shared_ptr<VulkanBuffer> m_TerrainInfo;
	std::shared_ptr<VulkanBuffer> m_CameraInfo;

	bool m_InWireframe = false;

	std::shared_ptr<VulkanTexture> m_NoiseMap;

	std::unordered_map<uint8_t, TerrainChunkIndexBuffer> m_LODIndexBuffer;

private: // DEBUG

#if DEBUG_TERRAIN_NORMALS == 1 
	void createNormalDebugRenderPass();
	std::shared_ptr<RenderPass> m_NormalsDebugRenderPass;
#endif
};