#pragma once

#include "Graphics/Vulkan/VulkanImage.h"
#include "TerrainVirtualMap.h"
#include "Terrain/Terrain.h"

#include <memory>

class VirtualTerrainSerializer
{
	friend class TerrainVirtualMap;

public:
	static void Serialize(const std::shared_ptr<VulkanImage>& terrain, const VirtualTerrainMapSpecification& spec, VirtualTextureType type,
		glm::uvec2 worldOffset = { 0u, 0u }, bool purgeContent = true);
	static void Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap, VirtualTextureType type);
};