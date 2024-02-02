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

VulkanTexture::VulkanTexture(const TextureSpecification& spec)
{
	m_Specification = spec;

	assert(m_Specification.LayerCount == m_Specification.Filepath.size());

	loadTextures();
}

VulkanTexture::~VulkanTexture()
{ }

void VulkanTexture::loadTextures()
{
	std::vector<std::shared_ptr<VulkanBaseBuffer>> stagingBuffers;
	for (uint32_t layer = 0; layer < m_Specification.LayerCount; layer++)
	{
		TextureInformation currentInfo{};

		stbi_uc* pixels = stbi_load(m_Specification.Filepath[layer].c_str(), &currentInfo.Width, &currentInfo.Height,
			&currentInfo.Channels, m_Specification.Channles);

		if (layer >= 1)
			assert(currentInfo.Width == m_Info.Width && currentInfo.Height == m_Info.Height);
		else
			m_Info = currentInfo;

		if (m_VulkanImage == nullptr)
		{
			if (m_Specification.Channles != 0)
				m_Info.Channels = m_Specification.Channles;

			m_Info.mipCount = m_Specification.GenerateMips ? VkUtils::calculateNumberOfMips(m_Info.Width, m_Info.Height) : 1;

			ImageSpecification imgSpec;
			imgSpec.Width = m_Info.Width;
			imgSpec.Height = m_Info.Height;
			imgSpec.Format = getFormat(m_Info.Channels);

			imgSpec.Mips = m_Info.mipCount;
			imgSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
			imgSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			imgSpec.UsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imgSpec.CreateSampler = m_Specification.CreateSampler;
			imgSpec.LayerCount = m_Specification.LayerCount;

			m_VulkanImage = std::make_shared<VulkanImage>(imgSpec);
			m_VulkanImage->Create();
		}

		VkDeviceSize imageSize = m_Info.Width * m_Info.Height * m_Info.Channels;

		if (!pixels)
			assert(false);

		BufferProperties stagingBufferProps;
		stagingBufferProps.bufferSize = (uint32_t)imageSize;
		stagingBufferProps.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferProps.MemProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		stagingBuffers.push_back(std::make_shared<VulkanBaseBuffer>(stagingBufferProps));

		void* data;
		stagingBuffers.back()->Map(data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		stagingBuffers.back()->Unmap();

		stbi_image_free(pixels);
	}

	VkImageSubresourceRange imgSubresource{};
	ImageSpecification texSpec = m_VulkanImage->getSpecification();
	imgSubresource.aspectMask = texSpec.Aspect;
	imgSubresource.layerCount = m_Specification.LayerCount;
	imgSubresource.levelCount = texSpec.Mips;
	imgSubresource.baseMipLevel = 0;

	VkUtils::transitionImageLayout(m_VulkanImage->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	for (uint32_t layer = 0; layer < m_Specification.LayerCount; layer++)
		m_VulkanImage->copyBuffer(*stagingBuffers[layer], layer);

	if (m_Info.mipCount == 1)
		VkUtils::transitionImageLayout(m_VulkanImage->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	else
		this->GenerateMips();
}

void VulkanTexture::GenerateMips()
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(VulkanDevice::getVulkanContext()->getGPU(), VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		assert(false); // nu suporta linear filtering
	}

	VkCommandBuffer commandBuffer = VkUtils::beginSingleTimeCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = m_VulkanImage->getVkImage();
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = m_Specification.LayerCount;
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
		blit.srcSubresource.layerCount = m_Specification.LayerCount;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = m_Specification.LayerCount;

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
