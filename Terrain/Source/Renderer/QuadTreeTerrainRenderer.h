#pragma once

#include "Terrain/Terrain.h"

#include "Graphics/Vulkan/VulkanRenderPipeline.h"
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
	inline static const std::string NAME								= "QuadTreeRendererMetrics";

	inline static const std::string GPU_UPDATE_VIRTUAL_MAP				= "_GpuUpdateVirtualMap";
	inline static const std::string GPU_UPDATE_STATUS_TEXTURE			= "_GpuUpdateVirtualStatusTexture";
	inline static const std::string GPU_UPDATE_INDIRECTION_TEXTURE		= "_GpuUpdateVirtualIndirectionTexture";
	inline static const std::string GPU_GENERATE_QUAD_TREE				= "_GpuVirtualGenerateQuadTree";
	inline static const std::string GPU_GENERATE_LOD_MAP				= "_GpuVirtualGenerateLODMap";
	inline static const std::string GPU_SET_NEIGHTBOURS					= "_GpuSetNeightbours";
	inline static const std::string GPU_FRUSTUM_CULLING					= "_GpuVirtualFrustumCulling";

	inline static const std::string CPU_LOAD_NEEDED_NODES				= "_CpuVirtualMapLoadNeededNodes";

	inline static const std::string RENDER_TERRAIN						= "_QuadTreeRenderTerrain";

	inline static uint32_t CHUNKS_LOADED_LAST_UPDATE					= 0;
	inline static uint32_t CHUNKS_LOADED_LAST_FRAME						= 0;
	inline static uint32_t MAX_VERTICES_RENDERED						= 0;
	inline static uint32_t MAX_INDICES_RENDERED							= 0;

	inline static uint32_t MEMORY_USED									= 0;
};

class QuadTreeTerrainRenderer
{
public:
	QuadTreeTerrainRenderer(const QuadTreeTerrainRendererSpecification& spec);
	~QuadTreeTerrainRenderer() = default;

	void refreshVirtualMap();
	void updateVirtualMap();

	void Render(const Camera& camera);

	void setWireframe(bool wireframe);

	const std::shared_ptr<VulkanImage>& getPhysicalTexture() { return m_VirtualMap->getPhysicalTexture(); }
	const std::shared_ptr<VulkanImage>& getStatusTexture() { return m_VirtualMap->getLoadStatusTexture(); }

private:
	void createLODMapPipeline();
	void createRenderPass();
	void createPipeline();
	void createNeighboursCommandPass();

	void createChunkIndexBuffer(uint8_t lod);

public:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	Camera SceneCamera;

private:
	const std::unique_ptr<TerrainData>& m_Terrain;
	std::shared_ptr<TerrainVirtualMap> m_VirtualMap;
	std::shared_ptr<QuadTreeLOD> m_QuadTreeLOD;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	VulkanRenderPass m_TerrainRenderPass;
	std::shared_ptr<VulkanRenderPipeline> m_TerrainPipeline;

	std::shared_ptr<VulkanImage> m_LODMap;
	VulkanComputePass m_LODMapComputePass;

	VulkanComputePass m_NeighboursComputePass;

	std::shared_ptr<VulkanBuffer> m_VertexBuffer;
	TerrainChunkIndexBuffer m_ChunkIndexBuffer;

	bool m_InWireframe = false;
	uint32_t m_BufferUsed = 0;
	uint32_t m_AvailableBuffer = 0;
};