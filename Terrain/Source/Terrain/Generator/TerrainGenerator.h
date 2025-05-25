#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "Graphics/Vulkan/VulkanPass.h"

#include "glm/glm.hpp"

#include <memory>
#include <array>

struct GenerationParameters
{
	int32_t Octaves = 1;
	float Amplitude = 0.5f;
	float Frequency = 0.0f;
	float Lacunarity = 0.2f;
	glm::vec2 Offset = glm::vec2(0.0);
	float _padding[1];
};

struct HydraulicErosionParameters
{
	int Simulations = 300;

	float Inertia = 0.1f;

	float ErosionSpeed = 0.3f;
	float DepositSpeed = 0.3f;
	float Evaporation = 0.01f;

	float InitalWater = 1.0;
	float IntialSpeed = 1.0;

	float MaxCapacity = 3.0;
};

class TerrainGenerator
{
public:
	TerrainGenerator(uint32_t width, uint32_t height);

	void Generate(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, uint32_t frameIndex);
	void Resize(uint32_t width, uint32_t height);

	void runHydraulicErosion(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, uint32_t frameIndex);

	std::shared_ptr<VulkanImage> getHeightMap() { return m_Noise; }
	std::shared_ptr<VulkanImage> getNormalMap() { return m_Normals; }
	std::shared_ptr<VulkanImage> getCompositionMap() { return m_Composition; }

	void notifyChange();

public:
	GenerationParameters Noise{ };
	HydraulicErosionParameters HydraulicErosion{ };

	bool RunHydraulicErosion = false;
	uint32_t HydraulicSimulations = 0;

private:
	void createShaders();
	void createImages();
	void createCompute();

private:
	uint32_t m_Width = 0, m_Height = 0;

	VulkanComputePass m_NoiseGenerationPass;
	VulkanComputePass m_CompositionPass;
	VulkanComputePass m_NormalComputePass;
	VulkanComputePass m_HydraulicErosionPass;

	std::shared_ptr<VulkanImage> m_Noise;
	std::shared_ptr<VulkanImage> m_Normals;
	std::shared_ptr<VulkanImage> m_Composition;

	GenerationParameters m_OldNoise{ };

	std::shared_ptr<VulkanBuffer> m_UniformBuffer;

	bool m_Valid = false;

};
