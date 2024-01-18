#include "VulkanTexture.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"
#include "VulkanBuffer.h"

#include "stb_image/std_image.h"
#include <cassert>

VkFormat getFormat(uint32_t channels)
{
	switch (channels)
	{
	case 1: return VK_FORMAT_R8_SRGB;
	case 2: 
	case 3: 
	case 4: return VK_FORMAT_R8G8B8A8_SRGB;
	default: assert(false);
	}
	return VK_FORMAT_UNDEFINED;
}

VulkanTexture::VulkanTexture(const std::string& filepath, uint32_t channels, bool generateMips)
{
	stbi_uc* pixels = stbi_load(filepath.c_str(), &m_Info.Width, &m_Info.Height,
		&m_Info.Channels, channels);

	if (channels != 0)
		m_Info.Channels = channels;

	VkDeviceSize imageSize = m_Info.Width * m_Info.Height * m_Info.Channels;

	m_Info.mipCount = generateMips ? VkUtils::calculateNumberOfMips(m_Info.Width, m_Info.Height) : 1;

	if (!pixels)
		throw(false);

	BufferProperties stagingBufferProps;
	stagingBufferProps.bufferSize = (uint32_t)imageSize;
	stagingBufferProps.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferProps.MemProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VulkanBaseBuffer stagingBuffer{ stagingBufferProps };

	void* data;
	stagingBuffer.Map(data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	stagingBuffer.Unmap();

	stbi_image_free(pixels);
	
	ImageSpecification imgSpec;
	imgSpec.Width = m_Info.Width;
	imgSpec.Height = m_Info.Height;
	imgSpec.Format = getFormat(m_Info.Channels);

	imgSpec.Mips = m_Info.mipCount;
	imgSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
	imgSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	imgSpec.UsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imgSpec.CreateSampler = true;

	m_VulkanImage = std::make_shared<VulkanImage>(imgSpec);
	m_VulkanImage->Create();

	VkImageSubresourceRange imgSubresource{};
	ImageSpecification texSpec = m_VulkanImage->getSpecification();
	imgSubresource.aspectMask = texSpec.Aspect;
	imgSubresource.layerCount = 1;
	imgSubresource.levelCount = texSpec.Mips;
	imgSubresource.baseMipLevel = 0;
	imgSubresource.baseArrayLayer = 0;

	VkUtils::transitionImageLayout(m_VulkanImage->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	m_VulkanImage->copyBuffer(stagingBuffer);

	if (m_Info.mipCount == 1)
		VkUtils::transitionImageLayout(m_VulkanImage->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	else
		this->generateMips();
}

VulkanTexture::~VulkanTexture()
{ }

void VulkanTexture::generateMips()
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(VulkanDevice::getVulkanContext()->getGPU(), VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw(false); // nu suporta linear filtering
	}

	VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = m_VulkanImage->getVkImage();
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = m_Info.Width;
	int32_t mipHeight = m_Info.Height;

	for (uint32_t i = 1; i < m_Info.mipCount; i++)
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
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			m_VulkanImage->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_VulkanImage->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

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

	barrier.subresourceRange.baseMipLevel = m_Info.mipCount - 1;
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
