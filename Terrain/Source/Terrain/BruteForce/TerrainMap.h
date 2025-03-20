#pragma once

#include <string>
#include <memory>

#include "Graphics/Vulkan/VulkanImage.h"

class TerrainMap
{
public:
	TerrainMap(const std::string& filepath);

	const std::shared_ptr<VulkanImage>& getMap() { return m_HeightMap; }

private:
	std::shared_ptr<VulkanImage> m_HeightMap;
};
