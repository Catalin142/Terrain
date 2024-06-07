#include "TerrainGenerationGUI.h"

#include <backends/imgui_impl_vulkan.h>

TerrainGenerationGUI::TerrainGenerationGUI(const std::shared_ptr<TerrainGenerator>& generator, const std::shared_ptr<Terrain>& terrain,
	uint32_t width, ImVec2 pos) :
	m_Generator(generator), Width(width), Position(pos), m_Terrain(terrain)
{ 
	m_Sampler = std::make_shared<VulkanSampler>(SamplerSpecification{});

	m_HeightMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		m_Generator->getHeightMap()->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);

	m_NormalMapDescriptor = ImGui_ImplVulkan_AddTexture(m_Sampler->Get(),
		m_Generator->getNormalMap()->getVkImageView(), VK_IMAGE_LAYOUT_GENERAL);

	m_LODTechniques["QuadTree"] = LODTechnique::QUAD_TREE;
	m_LODTechniques["Distance"] = LODTechnique::DISTANCE_BASED;
	m_LODTechniques["SinkingCircle"] = LODTechnique::SINKING_CIRCLE;
	m_LODNames.push_back("QuadTree");
	m_LODNames.push_back("Distance");
	m_LODNames.push_back("SinkingCircle");
}

void TerrainGenerationGUI::Render()
{
	ImGui::SetNextWindowPos(Position);
	ImGui::SetNextWindowSizeConstraints(ImVec2((float)Width, -1.0f), ImVec2((float)Width, FLT_MAX));
	ImGui::Begin("Terrain Generation", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::CollapsingHeader("Height map"))
	{
		bool shouldRegenerate = false;

		shouldRegenerate = ImGui::DragInt("Octaves", &m_Generator->Noise.Octaves, 0.3f, 1, 36) || shouldRegenerate;
		shouldRegenerate = ImGui::DragFloat("Amplitude", &m_Generator->Noise.Amplitude, 0.01f) || shouldRegenerate;
		shouldRegenerate = ImGui::DragFloat("Frequency", &m_Generator->Noise.Frequency, 0.01f) || shouldRegenerate;
		shouldRegenerate = ImGui::DragFloat("Lacunarity", &m_Generator->Noise.Lacunarity, 0.01f) || shouldRegenerate;
		shouldRegenerate = ImGui::DragFloat("Offset.x", &m_Generator->Noise.Offset.x, 0.5f) || shouldRegenerate;
		shouldRegenerate = ImGui::DragFloat("Offset.y", &m_Generator->Noise.Offset.y, 0.5f) || shouldRegenerate;

		if (shouldRegenerate)
			m_Generator->notifyChange();

		float size = (float)Width / 3.5f;

		if (ImGui::CollapsingHeader("Maps"))
		{
			ImGui::Image(m_HeightMapDescriptor, ImVec2{ size, size });
			ImGui::SameLine();
			ImGui::Image(m_NormalMapDescriptor, ImVec2{ size, size });
		}
	}

	if (ImGui::CollapsingHeader("Hydraulic erosion"))
	{
		ImGui::DragInt("Simulations", &m_Generator->HydraulicErosion.Simulations, 0.3f, 1, 4000);
		ImGui::DragFloat("Intertia", &m_Generator->HydraulicErosion.Inertia, 0.01f);
		ImGui::DragFloat("Erosion", &m_Generator->HydraulicErosion.ErosionSpeed, 0.01f);
		ImGui::DragFloat("Deposit", &m_Generator->HydraulicErosion.DepositSpeed, 0.01f);
		ImGui::DragFloat("Evaporation", &m_Generator->HydraulicErosion.Evaporation, 0.01f, 0.0, 1.0);
		ImGui::DragFloat("Water", &m_Generator->HydraulicErosion.InitalWater, 0.01f);
		ImGui::DragFloat("Speed", &m_Generator->HydraulicErosion.IntialSpeed, 0.01f);
		ImGui::DragFloat("Capacity", &m_Generator->HydraulicErosion.MaxCapacity, 0.01f);

		ImGui::Checkbox("Run", &m_Generator->RunHydraulicErosion);
		ImGui::Text("Simulations: %d", m_Generator->HydraulicSimulations);

		float size = (float)Width / 5.0f;
		if (ImGui::Button("Reset", ImVec2(size, 20.0f)))
			m_Generator->notifyChange();
	}

	if (ImGui::CollapsingHeader("Terrain LOD"))
	{
		static std::string currentItem = "Distance";

		if (ImGui::BeginCombo("##combo", currentItem.c_str())) 
		{
			for (size_t n = 0; n < m_LODNames.size(); n++)
			{
				bool isSelected = (currentItem == m_LODNames[n]);
				
				if (ImGui::Selectable(m_LODNames[n].c_str(), isSelected))
				{
					currentItem = m_LODNames[n];
					m_Terrain->setTechnique(m_LODTechniques[currentItem]);
				}
				
				if (isSelected)
					ImGui::SetItemDefaultFocus(); 			
			}
			ImGui::EndCombo();
		}
	}

	ImGui::End();
}
