#include "VulkanImage.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <iostream>
#include <cassert>

static uint32_t getFormatSizeInBytes(VkFormat format) {
	switch (format)
	{
	case VK_FORMAT_R32_SFLOAT: return 4;
	}
}

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memFlags)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(VulkanDevice::getVulkanContext()->getGPU(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & 1) == 1 && (memProperties.memoryTypes[i].propertyFlags & memFlags) == memFlags) {
			return i;
		}
	}

	assert(false);
	return 0;
}

VulkanImage::VulkanImage(const VulkanImageSpecification& spec) : m_Specification(spec)
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
		VkUtils::transitionImageLayout(m_Image, { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_Specification.Mips, 0, 1 }, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL);

		VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.image = m_Image;
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, m_Specification.Mips, 0, 1 };
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

	for (uint32_t mip = 0; mip < m_ImageViews.size(); mip++)
		vkDestroyImageView(device, m_ImageViews[mip], nullptr);

	if (m_Specification.CreateSampler)
		vkDestroySampler(device, m_Sampler, nullptr);

	m_ImageViews.clear();

	m_Image = nullptr;
	m_Sampler = nullptr;
	m_DeviceMemory = nullptr;
}

void VulkanImage::copyBuffer(const VulkanBuffer& buffer, uint32_t layer, const glm::uvec2& srcExtent, const glm::uvec2& offset)
{
	VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();
	copyBuffer(commandBuffer, buffer, layer, offset);
	VkUtils::endSingleTimeCommand(commandBuffer);
}

void VulkanImage::copyBuffer(VkCommandBuffer cmdBuffer, const VulkanBuffer& buffer, uint32_t layer, const glm::uvec2& srcExtent, const glm::uvec2& offset)
{
	glm::uvec2 extent = glm::uvec2(srcExtent);
	if (srcExtent.x == 0 || srcExtent.y == 0)
		extent = { m_Specification.Width, m_Specification.Height };

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = layer;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { (int32_t)offset.x, (int32_t)offset.y, 0 };
	region.imageExtent = {
		extent.x,
		extent.y,
		1
	};

	vkCmdCopyBufferToImage(
		cmdBuffer,
		buffer.getBuffer(),
		m_Image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);
}

void VulkanImage::batchCopyBuffer(VkCommandBuffer cmdBuffer, const VulkanBuffer& buffer, const std::vector<VkBufferImageCopy>& regions)
{
	vkCmdCopyBufferToImage(
		cmdBuffer,
		buffer.getBuffer(),
		m_Image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		regions.size(),
		regions.data()
	);
}

void VulkanImage::generateMips()
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(VulkanDevice::getVulkanContext()->getGPU(), VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		assert(false); // nu suporta linear filtering
	}

	VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = m_Image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = m_Specification.LayerCount;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = m_Specification.Width;
	int32_t mipHeight = m_Specification.Height;

	for (uint32_t i = 1; i < m_Specification.Mips; i++)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = m_Specification.LayerCount;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = m_Specification.LayerCount;

		vkCmdBlitImage(commandBuffer,
			m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_NEAREST);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1)	 mipWidth /= 2;
		if (mipHeight > 1)	 mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = m_Specification.Mips - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

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
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, m_Specification.MemoryType);

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
	if (m_Specification.MaxAnisotropy != 0.0f)
	{
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = m_Specification.MaxAnisotropy;
	}
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

	if (vkCreateImageView(device, &viewInfo, nullptr, &m_ImageViews.emplace_back()) != VK_SUCCESS)
	{
		std::cerr << "failed to create image view!\n";
		assert(false);
	}
	
	if (!m_Specification.ImageViewOnMips)
		return;

	for (uint32_t mip = 1; mip < m_Specification.Mips; mip++)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Image;
		viewInfo.viewType = m_Specification.LayerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_Specification.Format;
		viewInfo.subresourceRange.aspectMask = m_Specification.Aspect;
		viewInfo.subresourceRange.baseMipLevel = mip;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = m_Specification.LayerCount;

		VkDevice device = VulkanDevice::getVulkanDevice();

		if (vkCreateImageView(device, &viewInfo, nullptr, &m_ImageViews.emplace_back()) != VK_SUCCESS)
		{
			std::cerr << "failed to create image view!\n";
			assert(false);
		}
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


VulkanSampler::VulkanSampler(SamplerSpecification spec)
{
	m_Specification = spec;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = m_Specification.magFilter;
	samplerInfo.minFilter = m_Specification.minFilter;
	samplerInfo.addressModeU = m_Specification.addresMode;
	samplerInfo.addressModeV = m_Specification.addresMode;
	samplerInfo.addressModeW = m_Specification.addresMode;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	samplerInfo.anisotropyEnable = VK_FALSE;
	if (m_Specification.MaxAnisotropy != 0.0f)
	{
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = m_Specification.MaxAnisotropy;
	}

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxLod = float(m_Specification.Mips - 1);
	samplerInfo.minLod = 0.0f;

	if (vkCreateSampler(VulkanDevice::getVulkanDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
		assert(false);
}

VulkanSampler::~VulkanSampler()
{
	vkDestroySampler(VulkanDevice::getVulkanDevice(), m_Sampler, nullptr);
}


