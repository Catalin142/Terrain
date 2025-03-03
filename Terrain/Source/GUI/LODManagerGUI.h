#pragma once

#include "GUI/ProfilerGUI.h"

#include "Terrain/LODManager.h"

#include <memory>
#include <unordered_map>

class LODManagerGUI
{
public:
	LODManagerGUI(const std::unique_ptr<LODManager>& manager);

	void Render();
	void pushProfilerValues();

private:
	void createClipmapGUI();
	void renderClipmapGUI();

	void createQuadTreeGUI();
	void renderQuadTreeGUI();

	void createTessellationGUI();
	void renderTessellationGUI();

public:
	std::shared_ptr<VulkanRenderCommandBuffer> CommandBuffer;
	
	ImVec2 Position = ImVec2{ 10, 10 };
	ImVec2 Size = ImVec2{ 460, 900 };

private:
	const std::unique_ptr<LODManager>& m_LODManager;

	ProfilerGUI m_CPUProfiler{};
	ProfilerGUI m_GPUProfiler{};
	ProfilerGUI m_RenderProfiler{};
	std::string m_CurrentTechnique;

	std::unordered_map<std::string, LODTechnique> m_TechniqueMap;

	std::vector<VkDescriptorSet> m_MapDescriptors;
	std::vector<std::shared_ptr<VulkanImageView>> m_MapViews;
	std::shared_ptr<VulkanSampler> m_Sampler;

	bool m_Wireframe = false;
};