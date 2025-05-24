#pragma once

#include "VulkanBuffer.h"
#include "VulkanImage.h"

#include <vulkan/vulkan.h>

struct MemoryBarrierSpecification
{
	VkAccessFlagBits fromAccess;
	VkPipelineStageFlags fromStage;

	VkAccessFlagBits toAccess;
	VkPipelineStageFlags toStage;
};

namespace VulkanUtils
{
	VkCommandBuffer beginSingleTimeCommand();
	void endSingleTimeCommand(VkCommandBuffer buffer);
	void endSingleTimeCommand(VkCommandBuffer buffer, VkSubmitInfo info);
	void flushCommandBuffer(VkCommandBuffer buffer);
	
	void transitionImageLayout(VkImage image, VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	void transitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	uint32_t calculateNumberOfMips(uint32_t width, uint32_t height);

	void bufferMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& buffer,
		MemoryBarrierSpecification barrierSpecification);
	void imageMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanImage>& image,
		MemoryBarrierSpecification barrierSpecification);
}