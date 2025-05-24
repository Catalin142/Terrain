#include "TerrainMap.h"

#include "Graphics/Vulkan/VulkanUtils.h"

#include "stb_image/stb_image.h"

#define SIZE_OF_FLOAT16 2

TerrainMap::TerrainMap(const std::string& filepath, uint32_t width, uint32_t height)
{
	VulkanImageSpecification heightmapSpec;
	heightmapSpec.Width = width;
	heightmapSpec.Height = height;
	heightmapSpec.Format = VK_FORMAT_R16_UNORM;
	heightmapSpec.Channels = 1;
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
	m_HeightMap->loadFromFile({ filepath });

}
