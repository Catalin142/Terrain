#include "VulkanTexture.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"
#include "VulkanBuffer.h"

#include "stb_image/stb_image.h"
#include <cassert>
#include <vector>
#include <memory>

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

	assert(m_Specification.LayerCount <= MAX_NUMBER_OF_LAYERS);
	assert(m_Specification.LayerCount == m_Specification.Filepath.size());

	loadTextures();
}

VulkanTexture::~VulkanTexture()
{ }

void VulkanTexture::loadTextures()
{
	std::vector<std::shared_ptr<VulkanBuffer>> stagingBuffers;

	for (uint32_t layer = 0; layer < m_Specification.LayerCount; layer++)
	{
		TextureInformation currentInfo{};

		stbi_uc* pixels = stbi_load(m_Specification.Filepath[layer].c_str(), &currentInfo.Width, &currentInfo.Height,
			&currentInfo.Channels, m_Specification.Channles);

		if (layer >= 1)
			assert(currentInfo.Width == m_Info.Width && currentInfo.Height == m_Info.Height);
		else
			m_Info = currentInfo;

		m_Info.mipCount = m_Specification.GenerateMips ? VkUtils::calculateNumberOfMips(m_Info.Width, m_Info.Height) : 1;

		if (m_VulkanImage == nullptr)
		{
			if (m_Specification.Channles != 0)
				m_Info.Channels = m_Specification.Channles;

			VulkanImageSpecification imgSpec;
			imgSpec.Width = m_Info.Width;
			imgSpec.Height = m_Info.Height;
			imgSpec.Format = getFormat(m_Info.Channels);

			imgSpec.Mips = m_Info.mipCount;
			imgSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
			imgSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			imgSpec.UsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imgSpec.CreateSampler = false;
			imgSpec.LayerCount = m_Specification.LayerCount;

			m_VulkanImage = std::make_shared<VulkanImage>(imgSpec);
			m_VulkanImage->Create();
		}

		VkDeviceSize imageSize = m_Info.Width * m_Info.Height * m_Info.Channels;

		if (!pixels)
			assert(false);

		VulkanBufferProperties imageBufferProperties;
		imageBufferProperties.Size = (uint32_t)imageSize;
		imageBufferProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
		imageBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

		stagingBuffers.push_back(std::make_shared<VulkanBuffer>(imageBufferProperties));

		std::shared_ptr<VulkanBuffer>& lastBuffer = stagingBuffers.back();

		lastBuffer->Map();
		memcpy(lastBuffer->getMappedData(), pixels, static_cast<size_t>(imageSize));
		lastBuffer->Unmap();

		stbi_image_free(pixels);
	}

	VkImageSubresourceRange imgSubresource{};
	VulkanImageSpecification texSpec = m_VulkanImage->getSpecification();
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
		m_VulkanImage->generateMips();

	if (m_Specification.CreateSampler)
	{
		SamplerSpecification samplerSpec{};
		samplerSpec.MaxAnisotropy = m_Specification.MaxAnisotropy;
		samplerSpec.Mips = m_Info.mipCount;

		m_VulkanSampler = std::make_shared<VulkanSampler>(samplerSpec);
	}
}
