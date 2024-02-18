#pragma once

#include "Terrain/Terrain.h"

#include "Graphics/VulkanPipeline.h"
#include "Graphics/VulkanPass.h"
#include "Graphics/VulkanImage.h"
#include "Graphics/VulkanUniformBuffer.h"
#include "Graphics/VulkanRenderCommandBuffer.h"

#include "Scene/Camera.h"

#include <memory>

#define DEBUG_TERRAIN_NORMALS 0

class TerrainRenderer
{
public:
	TerrainRenderer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer);
	~TerrainRenderer() = default;

	void renderTerrain(const Camera& camera, const std::shared_ptr<Terrain>& terrain);

	void setRenderCommandBuffer(const std::shared_ptr<VulkanRenderCommandBuffer>& renderCommandBuffer) 
	{ m_RenderCommandBuffer = renderCommandBuffer; }

	const std::shared_ptr<VulkanImage> getOutput() { return m_TerrainRenderPass->Pipeline->getTargetFramebuffer()->getImage(0); }

	void onResize(uint32_t width, uint32_t height);

	void setWireframe(bool wireframe);

	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;
private:
	void initializeBuffers();

	void initializeRenderPass();

	void createDistanceLODRenderPass();
	void createDistanceLODPipeline();
	void renderDistanceLOD(const Camera& camera);

private:
	std::shared_ptr<Terrain> m_Terrain;

	std::shared_ptr<VulkanSampler> m_TerrainSampler;

	std::shared_ptr<VulkanRenderCommandBuffer> m_RenderCommandBuffer;

	std::shared_ptr<RenderPass> m_TerrainRenderPass;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	std::shared_ptr<VulkanUniformBufferSet> m_LodMapSet;
	std::shared_ptr<VulkanUniformBufferSet> m_TerrainChunksSet;
	std::shared_ptr<VulkanUniformBuffer> m_TerrainInfo;

	LODTechnique m_LastTechniqueUsed = LODTechnique::NONE;
	bool m_InWireframe = false;


private: // DEBUG

#if DEBUG_TERRAIN_NORMALS == 1 // Only on distance LOD
	void createNormalDebugRenderPass();
	std::shared_ptr<RenderPass> m_NormalsDebugRenderPass;
#endif
};