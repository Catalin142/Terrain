#pragma once

#include "Graphics/VulkanImage.h"
#include "Graphics/VulkanPass.h"

#include <memory>

struct GenerationParameters
{
	int32_t Octaves = 1;
	float Amplitude = 0.5f;
	float Frequency = 0.0f;
	float Gain = 0.5f;
	float Lacunarity = 0.2f;
	float b = 1.0f;
	float _padding[1];
};

struct CompositionParameters
{
	int8_t _padding[4];
};

class TerrainGenerator
{
public:
	TerrainGenerator(uint32_t width, uint32_t height);

	void Generate(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer);
	void Resize(uint32_t width, uint32_t height);

	std::shared_ptr<VulkanImage> getHeightMap() { return m_Noise; }
	std::shared_ptr<VulkanImage> getNormalMap() { return m_Normals; }
	std::shared_ptr<VulkanImage> getCompositionMap() { return m_Composition; }

	void notifyChange();

public:
	GenerationParameters Noise{ };
	CompositionParameters Composition{ };

private:
	void createShaders();
	void createImages();
	void createCompute();

private:
	uint32_t m_Width = 0, m_Height = 0;

	std::shared_ptr<VulkanComputePass> m_NoiseGenerationPass;
	std::shared_ptr<VulkanComputePass> m_CompositionPass;

	std::shared_ptr<VulkanImage> m_Noise;
	std::shared_ptr<VulkanImage> m_Normals;
	std::shared_ptr<VulkanImage> m_Composition;

	GenerationParameters m_OldNoise{ };
	CompositionParameters m_OldComposition{ };

	bool m_Valid = false;
};
