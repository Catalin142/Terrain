#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "TerrainVirtualMap.h"

#include <memory>

class VirtualTerrainSerializer
{
	friend class TerrainVirtualMap;

public:
	inline static std::shared_ptr<VulkanImage> auxImg;
	static void Init();

	static void Serialize(const std::shared_ptr<VulkanImage>& map, const VirtualTerrainMapSpecification& spec, VirtualTextureType type,
		glm::uvec2 worldOffset = { 0u, 0u }, bool purgeContent = true);
	static void Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap, VirtualTextureType type);
};