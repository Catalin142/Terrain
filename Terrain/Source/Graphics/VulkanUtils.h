#pragma once
#include <vulkan/vulkan.h>

namespace VkUtils
{
	VkCommandBuffer beginSingleTimeCommand();
	void endSingleTimeCommand(VkCommandBuffer buffer);

	void transitionImageLayout(VkImage image, VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout);

	uint32_t calculateNumberOfMips(uint32_t width, uint32_t height);
}