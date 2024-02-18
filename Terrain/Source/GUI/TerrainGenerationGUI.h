#pragma once

#include "Terrain/Generator/TerrainGenerator.h"
#include "Graphics/VulkanTexture.h"

#include "imgui/imgui.h"
#include <memory>

class TerrainGenerationGUI
{
public:
	TerrainGenerationGUI(const std::shared_ptr<TerrainGenerator>& generator, uint32_t width, ImVec2 pos);

	void Render();

private:
	std::shared_ptr<TerrainGenerator> m_Generator;
	std::shared_ptr<VulkanSampler> m_Sampler;

	uint32_t m_Width;
	ImVec2 m_Position;

	VkDescriptorSet m_HeightMapDescriptor;
	VkDescriptorSet m_NormalMapDescriptor;
	VkDescriptorSet m_CompositionMapDescriptor;
};
