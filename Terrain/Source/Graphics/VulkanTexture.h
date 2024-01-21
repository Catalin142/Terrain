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

struct TextureSpecification
{
	uint32_t Channles = 0;
	uint32_t generateMips = false;
	bool createSampler = true;
};

class VulkanTexture
{
public:
	VulkanTexture(const std::string& filepath, const TextureSpecification& spec);
	~VulkanTexture();

	VkImage getVkImage() { return m_VulkanImage->getVkImage(); }
	VkImageView getVkImageView() { return m_VulkanImage->getVkImageView(); }
	VkSampler getVkSampler() { return m_VulkanImage->getVkSampler(); }

	const TextureInformation& getInfo() { return m_Info; }

private:
	void generateMips();

private:
	std::shared_ptr<VulkanImage> m_VulkanImage;
	TextureInformation m_Info{};
	TextureSpecification m_Specification{};
};

struct SamplerSpecification
{
	VkFilter magFilter = VK_FILTER_LINEAR, minFilter = VK_FILTER_LINEAR;
	VkSamplerAddressMode addresMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	uint32_t Mips = 1;
};

class VulkanSampler
{
public:
	VulkanSampler(SamplerSpecification spec);
	~VulkanSampler();

	VkSampler Get() { return m_Sampler; }

private:
	VkSampler m_Sampler;
	SamplerSpecification m_Specification;
};
