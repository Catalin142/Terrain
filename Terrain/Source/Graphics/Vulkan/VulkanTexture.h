#pragma once

#include "VulkanImage.h"

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#define MAX_NUMBER_OF_LAYERS 8

struct TextureInformation
{
	int32_t Width = 0;
	int32_t Height = 0;
	int32_t Channels = 0;
	uint32_t mipCount = 1;
};

struct TextureSpecification
{
	uint32_t Channles = 0;
	uint32_t GenerateMips = false;
	bool CreateSampler = true;

	float MaxAnisotropy = 0.0f;

	uint32_t LayerCount = 1;
	std::vector<std::string> Filepath;
};

class VulkanTexture
{
public:
	VulkanTexture(const TextureSpecification& spec);
	~VulkanTexture();

	VkImage getVkImage() { return m_VulkanImage->getVkImage(); }
	VkImageView getVkImageView() { return m_VulkanImage->getVkImageView(); }
	VkSampler getVkSampler() { return m_VulkanSampler == nullptr ? VK_NULL_HANDLE : m_VulkanSampler->Get(); }

	const TextureInformation& getInfo() { return m_Info; }

private:
	void loadTextures();

private:
	std::shared_ptr<VulkanImage> m_VulkanImage = nullptr;
	std::shared_ptr<VulkanSampler> m_VulkanSampler = nullptr;
	TextureInformation m_Info{};
	TextureSpecification m_Specification{};
};