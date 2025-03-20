#pragma once

#include "Terrain/BruteForce/BruteForceData.h"
#include "Terrain/BruteForce/TerrainMap.h"

#include "Terrain/Techniques/BruteForceLOD.h"

#include "Terrain/Terrain.h"

#include "Graphics/Vulkan/VulkanFramebuffer.h"
#include "Graphics/Vulkan/VulkanFramebuffer.h"
#include "Graphics/Vulkan/VulkanPipeline.h"
#include "Graphics/Vulkan/VulkanPass.h"

#include "Scene/Camera.h"

#include <memory>
#include <string>

struct BruteForceTerrainRendererSpecification
{
	std::shared_ptr<VulkanFramebuffer> TargetFramebuffer;
	const std::unique_ptr<TerrainData>& Terrain;
	std::string HeightMapFilepath;

	BruteForceTerrainRendererSpecification(const std::unique_ptr<TerrainData>& terrain) : Terrain(terrain) {}
};

struct BruteForceRendererMetrics
{
	inline static const std::string NAME = "BruteForceRendererMetrics";

	inline static const std::string GPU_CREATE_CHUNK_BUFFER = "_GpuBruteForceCreateChunkBuffer";

	inline static const std::string RENDER_TERRAIN = "_BruteForceRenderTerrain";

	inline static uint32_t MAX_VERTICES_RENDERED = 0;

	inline static uint32_t MEMORY_USED = 0;
};

class BruteForceTerrainRenderer
{
public:
	BruteForceTerrainRenderer(const BruteForceTerrainRendererSpecification& specification);

	void Render(const Camera& camera);

	void Refresh(const Camera& camera);

	void setWireframe(bool wireframe);

	const std::shared_ptr<TerrainMap>& getTerrainMap() { return m_TerrainMap; }

private:
	void createRenderPass();
	void createPipeline();

	void createVertexBuffer();

public:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	glm::vec4 DistanceThreshold = { 1500.0f, 3000.0f, 4500.0f, 6000.0f };

private:
	const std::unique_ptr<TerrainData>& m_Terrain;
	std::shared_ptr<TerrainMap> m_TerrainMap;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	std::shared_ptr<BruteForceLOD> m_BruteForceLOD;

	RenderPass m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	std::shared_ptr<VulkanBuffer> m_VertexBuffer;

	bool m_InWireframe = false;
};