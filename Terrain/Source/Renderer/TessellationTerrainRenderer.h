#pragma once

#include "Terrain/Terrain.h"
#include "Terrain/Clipmap/ClipmapData.h"
#include "Terrain/Clipmap/TerrainClipmap.h"
#include "Terrain/Techniques/TessellationLOD.h"

#include "Scene/Camera.h"

#include "Graphics/Vulkan/VulkanFramebuffer.h"
#include "Graphics/Vulkan/VulkanRenderCommandBuffer.h"
#include "Graphics/Vulkan/VulkanPipeline.h"
#include "Graphics/Vulkan/VulkanPass.h"

#include <memory>

struct TessellationTerrainRendererSpecification
{
	std::shared_ptr<VulkanFramebuffer> TargetFramebuffer;
	const std::unique_ptr<TerrainData>& Terrain;
	ClipmapTerrainSpecification ClipmapSpecification;

	glm::vec3 CameraStartingPosition;
	uint32_t ControlPointSize = 16;
	uint32_t ControlPointsPerRow = 2; // power of 2

	TessellationTerrainRendererSpecification(const std::unique_ptr<TerrainData>& terrain) : Terrain(terrain) {}
};

struct TessellationRendererMetrics
{
	inline static const std::string NAME							= "TessellationRendererMetrics";

	inline static const std::string GPU_UPDATE_CLIPMAP				= "_GpuUpdateTessellationClipmapMap";
	inline static const std::string GPU_CREATE_VERTICAL_ERROR_MAP	= "_GpuCreateVerticalErrorMap";
	inline static const std::string GPU_GENERATE_AND_FRUSTUM_CULL	= "_GpuTessGenerateAndFrustumCull";

	inline static const std::string CPU_CREATE_CHUNK_BUFFER			= "_CpuTessellationCreateChunkBuffer";
	inline static const std::string CPU_LOAD_NEEDED_NODES			= "_CpuTessellationLoadNeededNodes";

	inline static const std::string RENDER_TERRAIN					= "_TessellationRenderTerrain";

	inline static uint32_t CHUNKS_LOADED_LAST_UPDATE				= 0;
	inline static uint32_t MAX_VERTICES_RENDERED					= 0;
	inline static uint32_t MAX_INDICES_RENDERED						= 0;

	inline static uint32_t MEMORY_USED								= 0;
};

class TessellationTerrainRenderer
{
public:
	TessellationTerrainRenderer(const TessellationTerrainRendererSpecification& spec);
	~TessellationTerrainRenderer() = default;

	void refreshClipmaps();
	void updateClipmaps();

	void Render(const Camera& camera);

	void setWireframe(bool wireframe);

	const std::shared_ptr<TerrainClipmap>& getClipmap() { return m_Clipmap; }
	const std::shared_ptr<VulkanImage>& getVerticalErrorMap() { return m_VerticalErrorMap; }

private:
	void createRenderPass();
	void createPipeline();

	void createVerticalErrorComputePass();

	void createVertexBuffer();

public:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	std::array<float, 6> Threshold = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

	Camera SceneCamera;

private:
	const std::unique_ptr<TerrainData>& m_Terrain;
	std::shared_ptr<TerrainClipmap> m_Clipmap;
	std::shared_ptr<TessellationLOD> m_TessellationLOD;

	std::shared_ptr<VulkanFramebuffer> m_TargetFramebuffer;

	RenderPass m_TerrainRenderPass;
	std::shared_ptr<VulkanPipeline> m_TerrainPipeline;

	VulkanComputePass m_VerticalErrorPass;
	std::shared_ptr<VulkanImage> m_VerticalErrorMap;

	std::shared_ptr<VulkanBuffer> m_TessellationSettings;

	std::shared_ptr<VulkanBuffer> m_VertexBuffer;
	uint32_t m_ChunksToRender = 0;

	std::shared_ptr<VulkanBuffer> m_ThresholdBuffer;

	bool m_InWireframe = false;
	bool m_VericalErrorMapGenerated = false;

	int32_t m_ControlPointSize = 0;
	int32_t m_ControlPointsPerRow = 0;
	uint32_t m_VerticalErrorMapSize = 0;

	std::shared_ptr<VulkanBuffer> m_FrustumBuffer;
};
