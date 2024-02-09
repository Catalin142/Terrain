#pragma once

#include "Terrain/Terrain.h"

#include "Graphics/VulkanPipeline.h"
#include "Graphics/VulkanRenderPass.h"
#include "Graphics/VulkanImage.h"
#include "Graphics/VulkanUniformBuffer.h"
#include "Graphics/VulkanRenderCommandBuffer.h"

#include "Scene/Camera.h"

#include <memory>

class TerrainRenderer
{
public:
	TerrainRenderer(const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer);
	~TerrainRenderer() = default;

	void renderTerrain(const Camera& camera, const std::shared_ptr<Terrain>& terrain);

	void setRenderCommandBuffer(const std::shared_ptr<VulkanRenderCommandBuffer>& renderCommandBuffer) 
	{ m_RenderCommandBuffer = renderCommandBuffer; }

	const std::shared_ptr<VulkanImage> getOutput() { return m_TerrainRenderPass->getOutput(0); }

	void onResize(uint32_t width, uint32_t height);

	void setWireframe(bool wireframe);

private:
	void initializeBuffers();

	void initializeRenderPass(const std::shared_ptr<Terrain>& terrain);

	void createDistanceLODRenderPass(const std::shared_ptr<Terrain>& terrain);
	void createDistanceLODPipeline();
	void renderDistanceLOD(const Camera& camera, const std::shared_ptr<Terrain>& terrain);

private:
	std::shared_ptr<VulkanRenderCommandBuffer> m_RenderCommandBuffer;

	std::shared_ptr<VulkanRenderPass> m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	std::shared_ptr<VulkanUniformBufferSet> m_CameraBufferSet;
	std::shared_ptr<VulkanUniformBufferSet> m_TerrainChunksSet;
	std::shared_ptr<VulkanUniformBufferSet> m_LodMapSet;

	LODTechnique m_LastTechniqueUsed = LODTechnique::NONE;
	bool m_InWireframe = false;
};