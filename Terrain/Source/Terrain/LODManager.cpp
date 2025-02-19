#include "LODManager.h"

#include "Graphics/Vulkan/VulkanDevice.h"

LODManager::LODManager(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer, Camera& cam)
	: m_Terrain(terrain), m_TargetFramebuffer(targetFramebuffer), m_Camera(cam)
{
	refreshTechnique();
}

void LODManager::refreshTechnique()
{
	switch (m_CurrentTechnique)
	{
	case LODTechnique::CLIPMAP:
		createClipmapRenderer();
		break;

	case LODTechnique::QUADTREE:
		createQuadTreeRenderer();
		break;
	}
}

void LODManager::preprocessTerrain()
{
	switch (m_CurrentTechnique)
	{
	case LODTechnique::CLIPMAP:
		ClipmapRenderer->refreshClipmaps(m_Camera);
		break;

	case LODTechnique::QUADTREE:
		QuadTreeRenderer->refreshVirtualMap(m_Camera);
		break;
	}
}

void LODManager::renderTerrain(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer)
{
	switch (m_CurrentTechnique)
	{
	case LODTechnique::CLIPMAP:
		ClipmapRenderer->CommandBuffer = cmdBuffer;

		ClipmapRenderer->updateClipmaps();
		ClipmapRenderer->Render(m_Camera);
		break;

	case LODTechnique::QUADTREE:
		QuadTreeRenderer->CommandBuffer = cmdBuffer;

		QuadTreeRenderer->updateVirtualMap();
		QuadTreeRenderer->Render(m_Camera);
		break;
	}
}

void LODManager::setTechnique(LODTechnique technique)
{
	vkDeviceWaitIdle(VulkanDevice::getVulkanDevice());
	
	if (m_CurrentTechnique == technique)
		return;

	m_CurrentTechnique = technique;
	refreshTechnique();
}

void LODManager::createClipmapRenderer()
{
	ClipmapTerrainRendererSpecification clipmapRendererSpecification(m_Terrain);
	clipmapRendererSpecification.TargetFramebuffer = m_TargetFramebuffer;
	clipmapRendererSpecification.CameraStartingPosition = m_Camera.getPosition();
	clipmapRendererSpecification.ClipmapSpecification = ClipmapSpecification;

	ClipmapRenderer = std::make_shared<ClipmapTerrainRenderer>(clipmapRendererSpecification);
}

void LODManager::createQuadTreeRenderer()
{
	QuadTreeTerrainRendererSpecification quadtreeRendererSpecification(m_Terrain);
	quadtreeRendererSpecification.TargetFramebuffer = m_TargetFramebuffer;
	quadtreeRendererSpecification.VirtualMapSpecification = VirtualMapSpecification;

	QuadTreeRenderer = std::make_shared<QuadTreeTerrainRenderer>(quadtreeRendererSpecification);
}

