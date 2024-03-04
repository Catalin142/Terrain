#include "VulkanImage.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <iostream>
#include <cassert>

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memFlags)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(VulkanDevice::getVulkanContext()->getGPU(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & 1) == 1 && (memProperties.memoryTypes[i].propertyFlags & memFlags) == memFlags) {
			return i;
		}
	}

	std::cerr << "Nu s a gasit memorie\n";
	assert(false);
}

VulkanImage::VulkanImage(const ImageSpecification& spec) : m_Specification(spec)
{ }

VulkanImage::~VulkanImage()
{
	Release();
}

void VulkanImage::Create()
{
	assert(m_Image == nullptr);

	createImage();

	if (m_Specification.UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
	{
		VkUtils::transitionImageLayout(m_Image, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL);

		VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.image = m_Image;
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = 0;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
		VkUtils::flushCommandBuffer(commandBuffer);
	}

	createView();

	if (m_Specification.CreateSampler)
		createSampler();
}

void VulkanImage::Release()
{
	VkDevice device = VulkanDevice::getVulkanDevice();

	vkDestroyImage(device, m_Image, nullptr);
	vkFreeMemory(device, m_DeviceMemory, nullptr);

	vkDestroyImageView(device, m_ImageView, nullptr);

	if (m_Specification.CreateSampler)
		vkDestroySampler(device, m_Sampler, nullptr);

	m_Image = nullptr;
	m_ImageView = nullptr;
	m_Sampler = nullptr;
	m_DeviceMemory = nullptr;
}

void VulkanImage::copyBuffer(const VulkanBaseBuffer& buffer, uint32_t layer)
{
	VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = layer;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		m_Specification.Width,
		m_Specification.Height,
		1
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer.getBuffer(),
		m_Image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	VkUtils::endSingleTimeCommand(commandBuffer);
}

void VulkanImage::createImage()
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_Specification.Width;
	imageInfo.extent.height = m_Specification.Height;
	imageInfo.extent.depth = 1;
	imageInfo.arrayLayers = m_Specification.LayerCount;
	imageInfo.format = m_Specification.Format;
	imageInfo.tiling = m_Specification.Tiling;
	imageInfo.usage = m_Specification.UsageFlags;
	imageInfo.samples = VkSampleCountFlagBits(m_Specification.Samples);
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.mipLevels = m_Specification.Mips;

	VkDevice device = VulkanDevice::getVulkanDevice();
	if (vkCreateImage(device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS)
	{
		std::cerr << "failed to create texture image!\n";
		assert(false);
	}

	VkMemoryRequirements memRequirements{};
	vkGetImageMemoryRequirements(device, m_Image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DeviceMemory) != VK_SUCCESS)
	{
		std::cerr << "failed to allocate image memory!\n";
		assert(false);
	}

	if (vkBindImageMemory(device, m_Image, m_DeviceMemory, 0) != VK_SUCCESS)
		assert(false);
}

void VulkanImage::createSampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	VulkanDevice* device = VulkanDevice::getVulkanContext();

	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = float(m_Specification.Mips - 1);

	if (vkCreateSampler(device->getLogicalDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
		assert(false);
}

void VulkanImage::createView()
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_Image;
	viewInfo.viewType = m_Specification.LayerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_Specification.Format;
	viewInfo.subresourceRange.aspectMask = m_Specification.Aspect;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = m_Specification.Mips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = m_Specification.LayerCount;

	VkDevice device = VulkanDevice::getVulkanDevice();

	if (vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
	{
		std::cerr << "failed to create image view!\n";
		assert(false);
	}
}

void VulkanImage::Resize(uint32_t width, uint32_t height)
{
	Release();

	m_Specification.Width = width;
	m_Specification.Height = height;

	Create();
}

VulkanImageView::VulkanImageView(const ImageViewSpecification& spec) : m_Specification(spec)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_Specification.Image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_Specification.Format;
	viewInfo.subresourceRange.aspectMask = m_Specification.Aspect;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = m_Specification.Mips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(VulkanDevice::getVulkanDevice(), &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
	{
		std::cerr << "failed to create image view!\n";
		assert(false);
	}
}

VulkanImageView::~VulkanImageView()
{
	if (m_ImageView != VK_NULL_HANDLE)
		vkDestroyImageView(VulkanDevice::getVulkanDevice(), m_ImageView, nullptr);
}

