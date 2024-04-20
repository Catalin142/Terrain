#pragma once

#include "glm/glm.hpp"
#include "Vulkan/VulkanImage.h"

#include <memory>

#define INVALID_PAGE -1

enum PageStatus : uint8_t
{
	AVAILABLE = 1 << 0,
	USED = 1 << 1,

	NEEDS_RENDERING = 1 << 2,
	RENDERERD = 1 << 3,
};

struct VirtualTextureSpecification
{
	glm::uvec2 VirtualTextureSize;
	
	uint32_t PageCountOnAxis;
	uint32_t PageSize;
	uint32_t LodCount;
};

class VirtualTexture
{
public:
	VirtualTexture(const VirtualTextureSpecification& spec);
	~VirtualTexture();

	void setPageStatus(const glm::uvec2& page, PageStatus status);
	glm::ivec2 allocatePage();
	void deallocatePage(const glm::uvec2& page);

	const std::shared_ptr<VulkanImage>& getIndirectionTexture() { return m_IndirectionTexture; }
	const std::shared_ptr<VulkanImage>& getPhysicalTexture() { return m_PhysicalTexture; }
	const std::shared_ptr<VulkanImage>& getVisualizationTexture() { return m_IndirectionVisualizationTexture; }

private:
	VirtualTextureSpecification m_Specification;

	std::shared_ptr<VulkanImage> m_IndirectionTexture;
	std::shared_ptr<VulkanImage> m_PhysicalTexture;
	std::shared_ptr<VulkanImage> m_IndirectionVisualizationTexture;

	uint8_t* m_PageStatus;
	glm::uvec2 m_FirstAvailablePage;

};
