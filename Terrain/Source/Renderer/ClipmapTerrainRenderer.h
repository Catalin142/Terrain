#pragma once

#include "Graphics/Vulkan/VulkanBuffer.h"
#include "Graphics/Vulkan/VulkanPass.h"
#include "Graphics/Vulkan/VulkanPipeline.h"

#include "Terrain/Terrain.h"
#include "Terrain/Clipmap/TerrainClipmap.h"
#include "Terrain/Techniques/ClipmapLOD.h"

#include "Scene/Camera.h"

#include <memory>

struct ClipmapTerrainRendererSpecification
{
	std::shared_ptr<VulkanFramebuffer> TargetFramebuffer;
	const std::unique_ptr<TerrainData>& Terrain;
	ClipmapTerrainSpecification ClipmapSpecification;

	glm::vec3 CameraStartingPosition;

	ClipmapTerrainRendererSpecification(const std::unique_ptr<TerrainData>& terrain) : Terrain(terrain) {}
};

struct ClipmapRendererMetrics
{
	inline static const std::string GPU_UPDATE_CLIPMAP			= "_GpuUpdateClipmapMap";

	inline static const std::string CPU_CREATE_CHUNK_BUFFER		= "_CpuClipmapCreateChunkBuffer";
	inline static const std::string CPU_LOAD_NEEDED_NODES		= "_CpuClipmapLoadNeededNodes";

	inline static const std::string RENDER_TERRAIN				= "_ClipmapRenderTerrain";
};

class ClipmapTerrainRenderer
{
public:
	ClipmapTerrainRenderer(const ClipmapTerrainRendererSpecification& spec);
	~ClipmapTerrainRenderer() = default;

	void refreshClipmaps(const Camera& camera);
	void updateClipmaps();

	void Render(const Camera& camera);

	void setWireframe(bool wireframe);

	std::shared_ptr<TerrainClipmap> m_Clipmap;
private:
	void createRenderPass();
	void createPipeline();

	void createChunkIndexBuffer(uint8_t lod);

public:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;

private:
	const std::unique_ptr<TerrainData>& m_Terrain;
	std::shared_ptr<ClipmapLOD> m_ClipmapLOD;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	std::shared_ptr<RenderPass> m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	std::shared_ptr<VulkanBuffer> m_TerrainInfoBuffer;

	std::shared_ptr<VulkanBuffer> m_VertexBuffer;
	TerrainChunkIndexBuffer m_ChunkIndexBuffer;
	uint32_t m_ChunksToRender = 0;

	bool m_InWireframe = false;
};