#include "TerrainGenerationGUI.h"

#include <backends/imgui_impl_vulkan.h>

TerrainGenerationGUI::TerrainGenerationGUI(const std::shared_ptr<TerrainGenerator>& generator, uint32_t width, ImVec2 pos) :
	m_Generator(generator), m_Width(width), m_Position(pos)
{ 
	m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

	m_HeightMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		m_Generator->getHeightMap()->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);

	m_NormalMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		m_Generator->getNormalMap()->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);

	m_CompositionMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		m_Generator->getCompositionMap()->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);
}

void TerrainGenerationGUI::Render()
{
	ImGui::SetNextWindowPos(m_Position);
	ImGui::SetNextWindowSizeConstraints(ImVec2(m_Width, -1.0f), ImVec2(m_Width, FLT_MAX));
	ImGui::Begin("Terrain Generation", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	bool shouldRegenerate = false;

	shouldRegenerate = ImGui::DragInt("Octaves", &m_Generator->Noise.Octaves, 0.3f, 1, 36) || shouldRegenerate;
	shouldRegenerate = ImGui::DragFloat("Amplitude", &m_Generator->Noise.Amplitude, 0.01f) || shouldRegenerate;
	shouldRegenerate = ImGui::DragFloat("Frequency", &m_Generator->Noise.Frequency, 0.01f) || shouldRegenerate;
	shouldRegenerate = ImGui::DragFloat("Gain", &m_Generator->Noise.Gain, 0.01f) || shouldRegenerate;
	shouldRegenerate = ImGui::DragFloat("Lacunarity", &m_Generator->Noise.Lacunarity, 0.01f) || shouldRegenerate;
	shouldRegenerate = ImGui::DragFloat("b", &m_Generator->Noise.b, 0.01f) || shouldRegenerate;

	if (shouldRegenerate)
		m_Generator->notifyChange();

	float size = (float)m_Width / 2.0f;

	ImGui::Text("Height map:");
	ImGui::Image(m_HeightMapDescriptor, ImVec2{ size, size });

	ImGui::Text("Normal map:");
	ImGui::Image(m_NormalMapDescriptor, ImVec2{ size, size });

	ImGui::Text("Composition map:");
	ImGui::Image(m_CompositionMapDescriptor, ImVec2{ size, size });

	ImGui::End();
}
