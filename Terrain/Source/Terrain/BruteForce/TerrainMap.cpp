#include "TerrainMap.h"

#include "Graphics/Vulkan/VulkanUtils.h"

#include "stb_image/stb_image.h"

#define SIZE_OF_FLOAT16 2

TerrainMap::TerrainMap(const std::string& filepath)
{
	int32_t width, height, channels;

	uint16_t* pixels = stbi_load_16(filepath.c_str(), &width, &height, &channels, 1);

	VulkanImageSpecification heightmapSpec;
	heightmapSpec.Width = width;
	heightmapSpec.Height = height;
	heightmapSpec.Format = VK_FORMAT_R16_UNORM;
	heightmapSpec.Mips = 1;
	heightmapSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
	heightmapSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	heightmapSpec.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	heightmapSpec.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	heightmapSpec.CreateSampler = false;
	heightmapSpec.LayerCount = 1;

	m_HeightMap = std::make_shared<VulkanImage>(heightmapSpec);
	m_HeightMap->Create();

	VkDeviceSize imageSize = width * height * channels * SIZE_OF_FLOAT16;

	if (!pixels)
		assert(false);

	VulkanBufferProperties imageBufferProperties;
	imageBufferProperties.Size = (uint32_t)imageSize;
	imageBufferProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	imageBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

	std::shared_ptr<VulkanBuffer> stagingBuffer = std::make_shared<VulkanBuffer>(imageBufferProperties);

	stagingBuffer->Map();
	memcpy(stagingBuffer->getMappedData(), pixels, static_cast<size_t>(imageSize));
	stagingBuffer->Unmap();

	stbi_image_free(pixels);

	VkImageSubresourceRange imgSubresource{};
	VulkanImageSpecification texSpec = m_HeightMap->getSpecification();
	imgSubresource.aspectMask = texSpec.Aspect;
	imgSubresource.layerCount = 1;
	imgSubresource.levelCount = texSpec.Mips;
	imgSubresource.baseMipLevel = 0;

	VkUtils::transitionImageLayout(m_HeightMap->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	m_HeightMap->copyBuffer(*stagingBuffer);
	VkUtils::transitionImageLayout(m_HeightMap->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

}
