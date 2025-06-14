#include "LODManager.h"

#include "Graphics/Vulkan/VulkanDevice.h"

LODManager::LODManager(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer, Camera& cam)
	: Terrain(terrain), m_TargetFramebuffer(targetFramebuffer), SceneCamera(cam)
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

	case LODTechnique::TESSELLATION:
		createTessellationRenderer();
		break;

	case LODTechnique::BRUTE_FORCE:
		createBruteForceRenderer();
		break;
	}
}

void LODManager::preprocessTerrain()
{
	switch (m_CurrentTechnique)
	{
	case LODTechnique::CLIPMAP:
		ClipmapRenderer->setWireframe(m_Wireframe);
		ClipmapRenderer->refreshClipmaps(SceneCamera);
		break;

	case LODTechnique::QUADTREE:
		QuadTreeRenderer->SceneCamera = SceneCamera;
		QuadTreeRenderer->setWireframe(m_Wireframe);
		QuadTreeRenderer->refreshVirtualMap();
		break;

	case LODTechnique::TESSELLATION:
		TessellationRenderer->SceneCamera = SceneCamera;
		TessellationRenderer->setWireframe(m_Wireframe);
		TessellationRenderer->refreshClipmaps();
		break;

	case LODTechnique::BRUTE_FORCE:
		BruteForceRenderer->setWireframe(m_Wireframe);
		break;
	}
}

void LODManager::renderTerrain(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, Camera cam, uint32_t frameIndex)
{
	switch (m_CurrentTechnique)
	{
	case LODTechnique::CLIPMAP:
		ClipmapRenderer->CommandBuffer = cmdBuffer;

		ClipmapRenderer->updateClipmaps(SceneCamera, frameIndex);
		ClipmapRenderer->Render(cam, frameIndex);
		break;

	case LODTechnique::QUADTREE:
		QuadTreeRenderer->CommandBuffer = cmdBuffer;

		QuadTreeRenderer->updateVirtualMap(frameIndex);
		QuadTreeRenderer->Render(cam, frameIndex);
		break;

	case LODTechnique::TESSELLATION:
		TessellationRenderer->CommandBuffer = cmdBuffer;

		TessellationRenderer->updateClipmaps(frameIndex);
		TessellationRenderer->Render(cam, frameIndex);
		break;

	case LODTechnique::BRUTE_FORCE:
		BruteForceRenderer->CommandBuffer = cmdBuffer;

		BruteForceRenderer->Refresh(SceneCamera, frameIndex);
		BruteForceRenderer->Render(cam, frameIndex);
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

void LODManager::setWireframe(bool wireframe)
{
	m_Wireframe = wireframe;
}

void LODManager::createClipmapRenderer()
{
	ClipmapTerrainRendererSpecification clipmapRendererSpecification(Terrain);
	clipmapRendererSpecification.TargetFramebuffer = m_TargetFramebuffer;
	clipmapRendererSpecification.CameraStartingPosition = SceneCamera.getPosition();
	clipmapRendererSpecification.ClipmapSpecification = ClipmapSpecification;

	ClipmapRenderer = std::make_shared<ClipmapTerrainRenderer>(clipmapRendererSpecification);
}

void LODManager::createQuadTreeRenderer()
{
	QuadTreeTerrainRendererSpecification quadtreeRendererSpecification(Terrain);
	quadtreeRendererSpecification.TargetFramebuffer = m_TargetFramebuffer;
	quadtreeRendererSpecification.VirtualMapSpecification = VirtualMapSpecification;

	QuadTreeRenderer = std::make_shared<QuadTreeTerrainRenderer>(quadtreeRendererSpecification);
}

void LODManager::createTessellationRenderer()
{
	TessellationTerrainRendererSpecification tessellationRendererSpecification(Terrain);
	tessellationRendererSpecification.TargetFramebuffer = m_TargetFramebuffer;
	tessellationRendererSpecification.CameraStartingPosition = SceneCamera.getPosition();
	tessellationRendererSpecification.ClipmapSpecification = ClipmapSpecification;

	TessellationRenderer = std::make_shared<TessellationTerrainRenderer>(tessellationRendererSpecification);
}

void LODManager::createBruteForceRenderer()
{
	BruteForceTerrainRendererSpecification spec(Terrain);
	spec.HeightMapFilepath = Terrain->getSpecification().TerrainFilepath;
	spec.TargetFramebuffer = m_TargetFramebuffer;

	BruteForceRenderer = std::make_shared<BruteForceTerrainRenderer>(spec);
}

