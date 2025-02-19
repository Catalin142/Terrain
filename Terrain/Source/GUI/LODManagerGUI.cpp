#include "LODManagerGUI.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

LODManagerGUI::LODManagerGUI(const std::unique_ptr<LODManager>& manager) : m_LODManager(manager)
{
	m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

	m_TechniqueMap["Clipmap"]  = LODTechnique::CLIPMAP;
	m_TechniqueMap["QuadTree"] = LODTechnique::QUADTREE;

	switch (m_LODManager->getCurrentTechnique())
	{
	case LODTechnique::CLIPMAP:
		m_CurrentTechnique = "Clipmap";
		createClipmapGUI();
		break;

	case LODTechnique::QUADTREE:
		m_CurrentTechnique = "QuadTree";
		createQuadTreeGUI();
		break;
	}

	m_CPUProfiler.Flush(m_CurrentTechnique);
	m_GPUProfiler.Flush(m_CurrentTechnique);
	m_RenderProfiler.Flush(m_CurrentTechnique);
}

void LODManagerGUI::Render()
{
	pushProfilerValues();

	ImGui::SetNextWindowPos(Position);
	ImGui::SetNextWindowSizeConstraints(Size, Size);
	ImGui::Begin("Terrain Renderer");
	
	std::string selectedTechnique = m_CurrentTechnique;

	if (ImGui::BeginCombo("##LOD Technique", selectedTechnique.c_str()))
	{
		for (const auto& [name, lodTechnique] : m_TechniqueMap)
		{
			bool isSelected = (selectedTechnique == name);
			if (ImGui::Selectable(name.c_str(), isSelected))
				selectedTechnique = name.c_str();
		}

		ImGui::EndCombo();
	}

	if (selectedTechnique != m_CurrentTechnique)
	{
		m_CurrentTechnique = selectedTechnique;
		m_LODManager->setTechnique(m_TechniqueMap[m_CurrentTechnique]);

		m_CPUProfiler.Flush("CPU-" + m_CurrentTechnique);
		m_GPUProfiler.Flush("GPU-" + m_CurrentTechnique);
		m_RenderProfiler.Flush("Render-" + m_CurrentTechnique);

		switch (m_LODManager->getCurrentTechnique())
		{
		case LODTechnique::CLIPMAP:
			createClipmapGUI();
			break;

		case LODTechnique::QUADTREE:
			createQuadTreeGUI();
			break;
		}
	}

	switch (m_LODManager->getCurrentTechnique())
	{
	case LODTechnique::CLIPMAP:
		renderClipmapGUI();
		break;

	case LODTechnique::QUADTREE:
		renderQuadTreeGUI();
		break;
	}

	ImGui::End();
}

void LODManagerGUI::pushProfilerValues()
{
	switch (m_LODManager->getCurrentTechnique())
	{
	case LODTechnique::CLIPMAP:
		m_CPUProfiler.pushValue("Push tasks", Instrumentor::Get().getTime(ClipmapRendererMetrics::CPU_LOAD_NEEDED_NODES), 0xffff00ff);
		m_CPUProfiler.pushValue("Chunk buffer", Instrumentor::Get().getTime(ClipmapRendererMetrics::CPU_CREATE_CHUNK_BUFFER), 0xffff0000);
		m_CPUProfiler.nextFrame();

		m_GPUProfiler.pushValue("Update clipmap", CommandBuffer->getTime(ClipmapRendererMetrics::GPU_UPDATE_CLIPMAP), 0xffffff00);
		m_GPUProfiler.nextFrame();

		m_RenderProfiler.pushValue("Render terrain", CommandBuffer->getTime(ClipmapRendererMetrics::RENDER_TERRAIN), 0xff00ff00);
		m_RenderProfiler.nextFrame();
		break;

	case LODTechnique::QUADTREE:
		m_CPUProfiler.pushValue("Push tasks", Instrumentor::Get().getTime(QuadTreeRendererMetrics::CPU_LOAD_NEEDED_NODES), 0xffff00ff);
		m_CPUProfiler.nextFrame();

		m_GPUProfiler.pushValue("Update VirtualMap", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_UPDATE_VIRTUAL_MAP), 0xffffff00);
		m_GPUProfiler.pushValue("Update status", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_UPDATE_STATUS_TEXTURE), 0xffff0000);
		m_GPUProfiler.pushValue("Update indirection", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_UPDATE_INDIRECTION_TEXTURE), 0xffff00ff);
		m_GPUProfiler.pushValue("Generate LODMap", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_GENERATE_LOD_MAP), 0xffffffff);
		m_GPUProfiler.pushValue("Generate QuadTree", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_GENERATE_QUAD_TREE), 0xff0000ff);
		m_GPUProfiler.pushValue("Set neighbours", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_SET_NEIGHTBOURS), 0xfff0000f);
		m_GPUProfiler.pushValue("Create render command", CommandBuffer->getTime(QuadTreeRendererMetrics::GPU_CREATE_INDIRECT_DRAW_COMMAND), 0xff0f00f0);
		m_GPUProfiler.nextFrame();

		m_RenderProfiler.pushValue("Render Terrain", CommandBuffer->getTime(QuadTreeRendererMetrics::RENDER_TERRAIN), 0xff00ff00);
		m_RenderProfiler.nextFrame();
		break;
	}
}

void LODManagerGUI::createClipmapGUI()
{
	const std::shared_ptr<VulkanImage> clipmap = m_LODManager->ClipmapRenderer->getClipmap()->getMap();

	m_MapViews.clear();
	m_MapDescriptors.clear();

	for (uint32_t lod = 0; lod < m_LODManager->getTerrainInfo().LODCount; lod++)
	{
		ImageViewSpecification imvSpec;
		imvSpec.Image = clipmap->getVkImage();
		imvSpec.Aspect = clipmap->getSpecification().Aspect;
		imvSpec.Format = clipmap->getSpecification().Format;
		imvSpec.Layer = lod;
		imvSpec.Mip = 0;

		m_MapViews.push_back(std::make_shared<VulkanImageView>(imvSpec));

		m_MapDescriptors.push_back(ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
			m_MapViews.back()->getImageView(), VK_IMAGE_LAYOUT_GENERAL));
	}
}

void LODManagerGUI::renderClipmapGUI()
{
	ImGui::Spacing();

	ImGui::Text("CPU Profiler");
	ImGui::BeginChild("CPU Profiler", { 560, 100 });
	m_CPUProfiler.Render({ 560, 100 });
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("GPU Profiler");
	ImGui::BeginChild("GPU Profiler", { 560, 100 });
	m_GPUProfiler.Render({ 560, 100 });
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("Render Profiler");
	ImGui::BeginChild("Render Profiler", { 560, 100 });
	m_RenderProfiler.Render({ 560, 100 });
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("Metrics");
	ImGui::Spacing();
	std::string lastLoaded = "Loaded last frame: " + std::to_string(ClipmapRendererMetrics::CHUNKS_LOADED_LAST_UPDATE);
	ImGui::Text(lastLoaded.c_str());

	std::string vertices = "Vertices: " + std::to_string(ClipmapRendererMetrics::MAX_VERTICES_RENDERED);
	std::string indices = "Indices: " + std::to_string(ClipmapRendererMetrics::MAX_INDICES_RENDERED);
	std::string memory = "Memory: " + std::to_string((float)ClipmapRendererMetrics::MEMORY_USED / 1048576.0f) + "MB";

	ImGui::Text(vertices.c_str());
	ImGui::Text(indices.c_str());
	ImGui::Text(memory.c_str());

	ImGui::Separator();

	uint32_t lodCount = m_LODManager->getTerrainInfo().LODCount;
	uint32_t clipmapResolution = m_LODManager->ClipmapSpecification.ClipmapSize;

	ImGui::Text(std::string("Maps: " + std::to_string(lodCount) + " * " + std::to_string(clipmapResolution) + "px").c_str());
	ImGui::Spacing();
	for (uint32_t lod = 0; lod < lodCount; lod++)
	{
		ImGui::Image(m_MapDescriptors[lod], ImVec2{128, 128});

		if (lod % 3 != 0 || lod == 0)
			ImGui::SameLine();
	}
}

void LODManagerGUI::createQuadTreeGUI()
{
	m_MapViews.clear();
	m_MapDescriptors.clear();

	const std::shared_ptr<VulkanImage>& virtualMap = m_LODManager->QuadTreeRenderer->getPhysicalTexture();

	ImageViewSpecification imvSpec;
	imvSpec.Image = virtualMap->getVkImage();
	imvSpec.Aspect = virtualMap->getSpecification().Aspect;
	imvSpec.Format = virtualMap->getSpecification().Format;
	imvSpec.Layer = 0;
	imvSpec.Mip = 0;

	m_MapViews.push_back(std::make_shared<VulkanImageView>(imvSpec));

	m_MapDescriptors.push_back(ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		m_MapViews.back()->getImageView(), VK_IMAGE_LAYOUT_GENERAL));
}

void LODManagerGUI::renderQuadTreeGUI()
{
	ImGui::Spacing();

	ImGui::Text("CPU Profiler");
	ImGui::BeginChild("CPU Profiler", { 560, 100 });
	m_CPUProfiler.Render({ 560, 100 });
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("GPU Profiler");
	ImGui::BeginChild("GPU Profiler", { 560, 200 });
	m_GPUProfiler.Render({ 560, 200 });
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("Render Profiler");
	ImGui::BeginChild("Render Profiler", { 560, 100 });
	m_RenderProfiler.Render({ 560, 100 });
	ImGui::EndChild();

	ImGui::Separator();

	ImGui::Text("Metrics");
	ImGui::Spacing();
	std::string lastLoaded = "Loaded last frame: " + std::to_string(QuadTreeRendererMetrics::CHUNKS_LOADED_LAST_UPDATE);
	ImGui::Text(lastLoaded.c_str());

	std::string vertices = "Vertices: " + std::to_string(QuadTreeRendererMetrics::MAX_VERTICES_RENDERED);
	std::string indices = "Indices: " + std::to_string(QuadTreeRendererMetrics::MAX_INDICES_RENDERED);
	std::string memory = "Memory: " + std::to_string((float)QuadTreeRendererMetrics::MEMORY_USED / 1048576.0f) + "MB";

	ImGui::Text(vertices.c_str());
	ImGui::Text(indices.c_str());
	ImGui::Text(memory.c_str());

	ImGui::Separator();

	ImGui::Text(std::string("Physical texture: " + std::to_string(m_LODManager->VirtualMapSpecification.PhysicalTextureSize) + "px").c_str());
	ImGui::Spacing();
	ImGui::Image(m_MapDescriptors[0], ImVec2{ 256, 256 });
}
