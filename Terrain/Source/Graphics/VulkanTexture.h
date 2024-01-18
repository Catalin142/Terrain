#pragma once

#include "VulkanImage.h"

#include <string>
#include <vulkan/vulkan.h>
#include <memory>

struct TextureInformation
{
	int32_t Width = 0;
	int32_t Height = 0;
	int32_t Channels = 0;
	uint32_t mipCount = 1;
};

class VulkanTexture
{
public:
	VulkanTexture(const std::string& filepath, uint32_t channels = 0, bool generateMips = false);
	~VulkanTexture();

	VkImage getVkImage() { return m_VulkanImage->getVkImage(); }
	VkImageView getVkImageView() { return m_VulkanImage->getVkImageView(); }
	VkSampler getVkSampler() { return m_VulkanImage->getVkSampler(); }

private:
	void generateMips();

private:
	std::shared_ptr<VulkanImage> m_VulkanImage;
	TextureInformation m_Info{};
};
