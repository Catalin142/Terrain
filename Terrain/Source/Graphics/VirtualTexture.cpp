#include "VirtualTexture.h"

VirtualTexture::VirtualTexture(const VirtualTextureSpecification& spec) : m_Specification(spec)
{
	{
		VulkanImageSpecification indirectionSpec{};
		// X on PhysicalTexture - 8 bits
		// Y on PhysicalTexture - 8 bits
		// Mip - 8 bits
		// Other - 8 bits
		indirectionSpec.Format = VK_FORMAT_R32_UINT;
		indirectionSpec.Width = m_Specification.VirtualTextureSize.x / m_Specification.PageSize;
		indirectionSpec.Height = m_Specification.VirtualTextureSize.y / m_Specification.PageSize;
		indirectionSpec.Mips = m_Specification.LodCount;
		indirectionSpec.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		indirectionSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		indirectionSpec.ImageViewOnMips = true;

		m_IndirectionTexture = std::make_shared<VulkanImage>(indirectionSpec);
		m_IndirectionTexture->Create();
	}

	{
		VulkanImageSpecification physicalSpec{};
		physicalSpec.Format = VK_FORMAT_R8G8B8A8_SRGB;
		physicalSpec.Width = m_Specification.PageCountOnAxis * m_Specification.PageSize;
		physicalSpec.Height = m_Specification.PageCountOnAxis * m_Specification.PageSize;
		physicalSpec.UsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
		physicalSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		m_PhysicalTexture = std::make_shared<VulkanImage>(physicalSpec);
		m_PhysicalTexture->Create();

		m_PageStatus = new uint8_t[m_Specification.PageCountOnAxis * m_Specification.PageCountOnAxis];
		memset(m_PageStatus, PageStatus::AVAILABLE, m_Specification.PageCountOnAxis * m_Specification.PageCountOnAxis * sizeof(uint8_t));
	}

	{
		VulkanImageSpecification visulizationSpec{};
		visulizationSpec.Format = VK_FORMAT_R8G8B8A8_SNORM;
		visulizationSpec.Width = m_Specification.VirtualTextureSize.x / m_Specification.PageSize;
		visulizationSpec.Height = m_Specification.VirtualTextureSize.y / m_Specification.PageSize;
		visulizationSpec.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		visulizationSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		m_IndirectionVisualizationTexture = std::make_shared<VulkanImage>(visulizationSpec);
		m_IndirectionVisualizationTexture->Create();
	}

	m_FirstAvailablePage = { 0U, 0U };
}

VirtualTexture::~VirtualTexture()
{
	delete[] m_PageStatus;
}

void VirtualTexture::setPageStatus(const glm::uvec2& page, PageStatus status)
{
	m_PageStatus[page.y * m_Specification.PageCountOnAxis + page.x] |= status;
}

glm::ivec2 VirtualTexture::allocatePage()
{
	uint32_t pageIndex = m_FirstAvailablePage.y * m_Specification.PageCountOnAxis + m_FirstAvailablePage.x;

	glm::ivec2 result = { INVALID_PAGE, INVALID_PAGE };

	if (m_PageStatus[pageIndex] & PageStatus::AVAILABLE)
	{
		result = m_FirstAvailablePage;
		m_FirstAvailablePage.x += 1;
		uint32_t clampedX = m_FirstAvailablePage.x % m_Specification.PageCountOnAxis;

		if (clampedX != m_FirstAvailablePage.x)
			m_FirstAvailablePage.y += 1, m_FirstAvailablePage.x = 0;

		m_FirstAvailablePage.y %= m_Specification.PageCountOnAxis;
	}

	else
	{
		for (uint32_t page = 0; page < m_Specification.PageCountOnAxis * m_Specification.PageCountOnAxis; page++)
			if (m_PageStatus[page] & PageStatus::AVAILABLE)
			{
				result.x = page % m_Specification.PageCountOnAxis;
				result.y = page / m_Specification.PageCountOnAxis;
				m_FirstAvailablePage.x = (page + 1) % m_Specification.PageCountOnAxis;
				m_FirstAvailablePage.y = (page + 1) / m_Specification.PageCountOnAxis;
				break;
			}
	}

	if (result == glm::ivec2(-1, -1))
		return result;

	pageIndex = result.y * m_Specification.PageCountOnAxis + result.x;
	m_PageStatus[pageIndex] &= ~PageStatus::AVAILABLE;
	m_PageStatus[pageIndex] |= PageStatus::USED;

	return result;
}

void VirtualTexture::deallocatePage(const glm::uvec2& page)
{
	uint32_t pageIndex = page.y * m_Specification.PageCountOnAxis + page.x;

	m_PageStatus[pageIndex] &= ~PageStatus::USED;
	m_PageStatus[pageIndex] |= PageStatus::AVAILABLE;
}
