#pragma once

#include "VulkanImage.h"

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

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
	VkSampler getVkSampler() { return m_VulkanImage->getVkSampler(); }

	const TextureInformation& getInfo() { return m_Info; }

private:
	void loadTextures();
	void GenerateMips();

private:
	std::shared_ptr<VulkanImage> m_VulkanImage = nullptr;
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
