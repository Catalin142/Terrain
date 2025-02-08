#pragma once

#include "glm/glm.hpp"
#include "VulkanBuffer.h"

#include <vector>
#include <vulkan/vulkan.h>

struct VulkanImageSpecification
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Samples = 1;
	uint32_t Mips = 1;
	uint32_t LayerCount = 1;

	float MaxAnisotropy = 0.0f;

	VkFormat Format = VK_FORMAT_UNDEFINED;
	VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags UsageFlags;
	VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_NONE;
	VkMemoryPropertyFlagBits MemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	
	bool Transfer = false;
	bool CreateSampler = false;
	bool ImageViewOnMips = false;
};

class VulkanImage
{
public:
	VulkanImage(const VulkanImageSpecification& spec);
	~VulkanImage();

	VulkanImage(const VulkanImage&) = delete;
	void operator=(const VulkanImage&) = delete;

	void Create();
	void Release();
	void Resize(uint32_t width, uint32_t height);

	VkImage getVkImage() const { return m_Image; }
	VkDeviceMemory getVkDeviceMemory() const { return m_DeviceMemory; }
	VkImageView getVkImageView(uint32_t mip = 0) const { return m_ImageViews[mip]; }
	VkSampler getVkSampler() const { return m_Sampler; }

	const VulkanImageSpecification& getSpecification() { return m_Specification; }
	void copyBuffer(const VulkanBuffer& buffer, uint32_t layer = 0, const glm::uvec2& srcExtent = glm::uvec2(0u, 0u),
		const glm::uvec2& offset = glm::ivec2(0u, 0u));
	void copyBuffer(VkCommandBuffer cmdBuffer, const VulkanBuffer& buffer, uint32_t layer = 0, const glm::uvec2& srcExtent = glm::uvec2(0u, 0u),
		const glm::uvec2& offset = glm::ivec2(0u, 0u));
	void batchCopyBuffer(VkCommandBuffer cmdBuffer, const VulkanBuffer& buffer, const std::vector<VkBufferImageCopy>& regions);

	void generateMips(VkFilter filter = VK_FILTER_LINEAR);

private:
	void createImage();
	void createSampler();
	void createView();

private:
	VulkanImageSpecification m_Specification;

	VkImage m_Image = nullptr;
	std::vector<VkImageView> m_ImageViews;
	VkDeviceMemory m_DeviceMemory = nullptr;
	VkSampler m_Sampler = nullptr;
};

struct ImageViewSpecification
{
	VkImage Image;
	VkFormat Format;
	uint32_t Mip;
	uint32_t Layer;
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

struct SamplerSpecification
{
	VkFilter magFilter = VK_FILTER_LINEAR, minFilter = VK_FILTER_LINEAR;
	VkSamplerAddressMode addresMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	uint32_t Mips = 1;

	float MaxAnisotropy = 0.0f;
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
