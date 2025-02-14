#pragma once

#include "Terrain/Terrain.h"

#include "Graphics/Vulkan/VulkanPipeline.h"
#include "Graphics/Vulkan/VulkanPass.h"
#include "Graphics/Vulkan/VulkanImage.h"
#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Graphics/Vulkan/VulkanRenderCommandBuffer.h"

#include "Core/Instrumentor.h"
#include "Terrain/Techniques/QuadTreeLOD.h"

#include "Scene/Camera.h"

#include <memory>

struct QuadTreeTerrainRendererSpecification
{
	std::shared_ptr<VulkanFramebuffer> TargetFramebuffer;
	const std::unique_ptr<TerrainData>& Terrain;
	VirtualTerrainMapSpecification VirtualMapSpecification;

	QuadTreeTerrainRendererSpecification(const std::unique_ptr<TerrainData>& terrain) : Terrain(terrain) {}
};

struct QuadTreeRendererMetrics
{
	inline static const std::string GPU_UPDATE_VIRTUAL_MAP				= "_GpuUpdateVirtualMap";
	inline static const std::string GPU_UPDATE_STATUS_TEXTURE			= "_GpuUpdateVirtualStatusTexture";
	inline static const std::string GPU_UPDATE_INDIRECTION_TEXTURE		= "_GpuUpdateVirtualIndirectionTexture";
	inline static const std::string GPU_GENERATE_QUAD_TREE				= "_GpuVirtualGenerateQuadTree";
	inline static const std::string GPU_GENERATE_LOD_MAP				= "_GpuVirtualGenerateLODMap";
	inline static const std::string GPU_CREATE_INDIRECT_DRAW_COMMAND	= "_GpuVirtualGenerateDrawCommand";

	inline static const std::string CPU_LOAD_NEEDED_NODES				= "_CpuVirtualMapLoadNeededNodes";

	inline static const std::string RENDER_TERRAIN						= "_QuadTreeRenderTerrain";
};

class QuadTreeTerrainRenderer
{
public:
	QuadTreeTerrainRenderer(const QuadTreeTerrainRendererSpecification& spec);
	~QuadTreeTerrainRenderer() = default;

	void refreshVirtualMap(const Camera& camera);
	void updateVirtualMap();

	void Render(const Camera& camera);

	void setWireframe(bool wireframe);

	std::shared_ptr<TerrainVirtualMap> m_VirtualMap;
private:
	void createLODMapPipeline();
	void createRenderPass();
	void createPipeline();
	void createIndirectCommandPass();

	void createChunkIndexBuffer(uint8_t lod);

public:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;

private:
	const std::unique_ptr<TerrainData>& m_Terrain;
	std::shared_ptr<QuadTreeLOD> m_QuadTreeLOD;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	std::shared_ptr<RenderPass> m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	std::shared_ptr<VulkanImage> m_LODMap;
	std::shared_ptr<VulkanComputePass> m_LODMapComputePass;

	std::shared_ptr<VulkanBuffer> m_TerrainInfo;

	TerrainChunkIndexBuffer m_ChunkIndexBuffer;

	// As we keep all data on the GPU, we can't issue a command directly from the CPU without fetching memory from the GPU.
	// The best solution would be to create the draw command on the GPU
	std::shared_ptr<VulkanComputePass> m_IndirectPass;
	std::shared_ptr<VulkanBufferSet> m_DrawIndirectCommandsSet;

	bool m_InWireframe = false;
};