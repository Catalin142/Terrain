#pragma once

#include "Renderer/BruteForceTerrainRenderer.h"
#include "Renderer/ClipmapTerrainRenderer.h"
#include "Renderer/QuadTreeTerrainRenderer.h"
#include "Renderer/TessellationTerrainRenderer.h"

#include "Terrain/Terrain.h"

#include <array>

enum class LODTechnique
{
	BRUTE_FORCE,
	QUADTREE,
	CLIPMAP,
	TESSELLATION,
};

class LODManager
{
public:
	LODManager(const std::unique_ptr<TerrainData>& terrain, const std::shared_ptr<VulkanFramebuffer>& targetFramebuffer, Camera& cam);
	~LODManager() = default;

	void refreshTechnique();

	void preprocessTerrain();
	void renderTerrain(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, Camera cam, uint32_t frameIndex);

	LODTechnique getCurrentTechnique() { return m_CurrentTechnique; }

	void setTechnique(LODTechnique technique);
	void setWireframe(bool wireframe);

	const TerrainInfo& getTerrainInfo() { return Terrain->getInfo(); }

private:
	void createClipmapRenderer();
	void createQuadTreeRenderer();
	void createTessellationRenderer();
	void createBruteForceRenderer();

public:
	std::shared_ptr<ClipmapTerrainRenderer> ClipmapRenderer = nullptr;
	std::shared_ptr<QuadTreeTerrainRenderer> QuadTreeRenderer = nullptr;
	std::shared_ptr<TessellationTerrainRenderer> TessellationRenderer = nullptr;
	std::shared_ptr<BruteForceTerrainRenderer> BruteForceRenderer = nullptr;

	ClipmapTerrainSpecification ClipmapSpecification{ 1024 };
	VirtualTerrainMapSpecification VirtualMapSpecification{ 1024 * 4,  std::array<uint32_t, MAX_LOD>{8, 8, 8, 8, 8, 8} };

	const std::unique_ptr<TerrainData>& Terrain;

	Camera& SceneCamera;

private:
	LODTechnique m_CurrentTechnique = LODTechnique::BRUTE_FORCE;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	bool m_Wireframe = false;
};