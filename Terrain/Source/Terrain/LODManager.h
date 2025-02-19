#pragma once

#include "Renderer/ClipmapTerrainRenderer.h"
#include "Renderer/QuadTreeTerrainRenderer.h"

#include "Terrain/Terrain.h"

#include <array>

enum class LODTechnique
{
	CLIPMAP,
	QUADTREE
};

class LODManager
{
public:
	LODManager(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer, Camera& cam);
	~LODManager() = default;

	void refreshTechnique();

	void preprocessTerrain();
	void renderTerrain(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer);

	LODTechnique getCurrentTechnique() { return m_CurrentTechnique; }

	void setTechnique(LODTechnique technique);

	const TerrainInfo& getTerrainInfo() { return m_Terrain->getInfo(); }

private:
	void createClipmapRenderer();
	void createQuadTreeRenderer();

public:
	std::shared_ptr<ClipmapTerrainRenderer> ClipmapRenderer = nullptr;
	std::shared_ptr<QuadTreeTerrainRenderer> QuadTreeRenderer = nullptr;

	ClipmapTerrainSpecification ClipmapSpecification{ 1024 };
	VirtualTerrainMapSpecification VirtualMapSpecification{ 2048,  std::array<uint32_t, MAX_LOD>{8, 8, 8, 0, 0} };

private:
	LODTechnique m_CurrentTechnique = LODTechnique::CLIPMAP;
	Camera& m_Camera;

	const std::unique_ptr<TerrainData>& m_Terrain;
	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;
};