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

struct TerrainChunkIndexBuffer
{
	std::shared_ptr<VulkanBuffer> IndexBuffer;
	uint32_t IndicesCount = 0;
};

struct QuadTreeTerrainRendererSpecification
{
	std::shared_ptr<VulkanFramebuffer> targetFramebuffer;
	std::shared_ptr<Terrain> Terrain;
	VirtualTerrainMapSpecification VirtualMapSpecification;
};

struct QuadTreeRendererMetrics
{
	inline static const std::string GPU_UPDATE_VIRTUAL_MAP				= "_GpuUpdateVirtualMap";
	inline static const std::string GPU_UPDATE_STATUS_TEXTURE			= "_GpuUpdateVirtualStatusTexture";
	inline static const std::string GPU_UPDATE_INDIRECTION_TEXTURE		= "_GpuUpdateVirtualIndirectionTexture";
	inline static const std::string GPU_GENERATE_QUAD_TREE				= "_GpuGenerateQuadTree";
	inline static const std::string GPU_GENERATE_LOD_MAP				= "_GpuGenerateLODMap";
	inline static const std::string GPU_CREATE_INDIRECT_DRAW_COMMAND	= "_GpuGenerateDrawCommand";

	inline static const std::string CPU_LOAD_NEEDED_NODES				= "_CpuLoadNeededNodes";

	inline static const std::string RENDER_TERRAIN						= "_QuadTreeRenderTerrain";
};

class QuadTreeTerrainRenderer
{
public:
	QuadTreeTerrainRenderer(const QuadTreeTerrainRendererSpecification& spec);
	~QuadTreeTerrainRenderer() = default;

	void updateVirtualMap(const glm::vec3& camPosition);

	void Precompute();
	void Render(const Camera& camera);

	const std::shared_ptr<VulkanImage> getOutput() { return m_TerrainRenderPass->Pipeline->getTargetFramebuffer()->getImage(0); }

	void setWireframe(bool wireframe);

	void createChunkIndexBuffer(uint8_t lod);

private:
	void createLODMapPipeline();
	void createRenderPass();
	void createPipeline();
	void createIndirectCommandPass();

public:
	std::shared_ptr<VulkanRenderCommandBuffer> commandBuffer;

private:
	std::shared_ptr<Terrain> m_Terrain;
	std::shared_ptr<TerrainVirtualMap> m_VirtualMap;
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