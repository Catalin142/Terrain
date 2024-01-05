#pragma once

#include "VulkanBaseBuffer.h"

#include <vulkan/vulkan.h>

struct ImageSpecification
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Samples = 1;
	uint32_t Mips = 1;

	VkFormat Format = VK_FORMAT_UNDEFINED;
	VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags UsageFlags;
	VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_NONE;
	
	bool Transfer = false;
	bool CreateSampler = false;
};

class VulkanImage
{
public:
	VulkanImage(const ImageSpecification& spec);
	~VulkanImage();

	VulkanImage(const VulkanImage&) = delete;
	void operator=(const VulkanImage&) = delete;

	void Create();
	void Release();
	void Resize(uint32_t width, uint32_t height);

	VkImage getVkImage() const { return m_Image; }
	VkImageView getVkImageView() const { return m_ImageView; }
	VkSampler getVkSampler() const { return m_Sampler; }

	const ImageSpecification& getSpecification() { return m_Specification; }
	void copyBuffer(const VulkanBaseBuffer& buffer);

private:
	void createImage();
	void createSampler();
	void createView();

private:
	ImageSpecification m_Specification;

	VkImage m_Image = nullptr;
	VkImageView m_ImageView = nullptr;
	VkDeviceMemory m_DeviceMemory = nullptr;
	VkSampler m_Sampler = nullptr;
};

struct ImageViewSpecification
{
	VkImage Image;
	VkFormat Format;
	uint32_t Mips;
	VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_NONE;
};

class VulkanImageView
{
public:
	VulkanImageView(const ImageViewSpecification& spec);
	~VulkanImageView();

	VulkanImageView(const VulkanImageView&) = delete;
	void operator=(const VulkanImageView&)  = delete;

	VkImageView getImageView() const { return m_ImageView; }

private:
	ImageViewSpecification m_Specification;
	VkImageView m_ImageView = VK_NULL_HANDLE;
};