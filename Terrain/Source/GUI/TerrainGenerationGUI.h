#pragma once

#include "Terrain/Generator/TerrainGenerator.h"
#include "Terrain/Terrain.h"

#include "Graphics/Vulkan/VulkanTexture.h"
#include "imgui/imgui.h"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

class TerrainGenerationGUI
{
public:
	TerrainGenerationGUI(const std::shared_ptr<TerrainGenerator>& generator, const std::shared_ptr<Terrain>& terrain,
		uint32_t width, ImVec2 pos);

	void Render();

public:
	ImVec2 Position;
	uint32_t Width;

private:
	std::shared_ptr<TerrainGenerator> m_Generator;
	std::shared_ptr<VulkanSampler> m_Sampler;
	std::shared_ptr<Terrain> m_Terrain;

	VkDescriptorSet m_HeightMapDescriptor;
	VkDescriptorSet m_NormalMapDescriptor;

	std::unordered_map<std::string, LODTechnique> m_LODTechniques;
	std::vector<std::string> m_LODNames;
};
