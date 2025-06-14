#include "VulkanUtils.h"

#include "VulkanDevice.h"

#include "glm/glm.hpp"
#include "glm/gtc/integer.hpp"

#include <mutex>

static std::mutex queueMutex;

VkCommandBuffer VulkanUtils::beginSingleTimeCommand()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = VulkanDevice::getGraphicsCommandPool();
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(VulkanDevice::getVulkanDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanUtils::endSingleTimeCommand(VkCommandBuffer buffer)
{
	vkEndCommandBuffer(buffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &buffer;

	VulkanDevice* deviceContext = VulkanDevice::getVulkanContext();

	{
		vkQueueSubmit(deviceContext->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(deviceContext->getGraphicsQueue());
	}

	vkFreeCommandBuffers(deviceContext->getLogicalDevice(), VulkanDevice::getGraphicsCommandPool(), 1, &buffer);
}

void VulkanUtils::endSingleTimeCommand(VkCommandBuffer buffer, VkSubmitInfo info)
{
	vkEndCommandBuffer(buffer);

	VulkanDevice* deviceContext = VulkanDevice::getVulkanContext();

	{
		vkQueueSubmit(deviceContext->getGraphicsQueue(), 1, &info, VK_NULL_HANDLE);
		vkQueueWaitIdle(deviceContext->getGraphicsQueue());
	}

	vkFreeCommandBuffers(deviceContext->getLogicalDevice(), VulkanDevice::getGraphicsCommandPool(), 1, &buffer);
}

void VulkanUtils::flushCommandBuffer(VkCommandBuffer commandBuffer)
{
	if (commandBuffer == VK_NULL_HANDLE)
		return;

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = 0; 

	VkFence fence;
	if (vkCreateFence(VulkanDevice::getVulkanDevice(), &fenceInfo, nullptr, &fence))
		assert(false);

	{
		vkQueueSubmit(VulkanDevice::getVulkanContext()->getGraphicsQueue(), 1, &submitInfo, fence);
		vkWaitForFences(VulkanDevice::getVulkanDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
	}
	vkDestroyFence(VulkanDevice::getVulkanDevice(), fence, nullptr);
	vkFreeCommandBuffers(VulkanDevice::getVulkanDevice(), VulkanDevice::getVulkanContext()->getGraphicsCommandPool(), 1, &commandBuffer);
}

void VulkanUtils::transitionImageLayout(VkImage image, VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout,
	VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommand();
	transitionImageLayout(commandBuffer, image, subresourceRange, oldLayout, newLayout, srcStageMask, dstStageMask);
	endSingleTimeCommand(commandBuffer);
}

void VulkanUtils::transitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = subresourceRange;

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	switch (oldLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		barrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		break;
	}

	switch (newLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		if (barrier.srcAccessMask == 0)
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		break;
	}

	vkCmdPipelineBarrier(
		cmdBuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

}

uint32_t VulkanUtils::calculateNumberOfMips(uint32_t width, uint32_t height)
{
	return (uint32_t)glm::floor(glm::log2(glm::min(width, height))) + 1;
}

void VulkanUtils::bufferMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& buffer, MemoryBarrierSpecification barrierSpecification)
{
	VkBufferMemoryBarrier bufferMemoryBarrier = {};
	bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemoryBarrier.buffer = buffer->getBuffer();
	bufferMemoryBarrier.offset = 0;
	bufferMemoryBarrier.size = VK_WHOLE_SIZE;
	bufferMemoryBarrier.srcAccessMask = barrierSpecification.fromAccess;
	bufferMemoryBarrier.dstAccessMask = barrierSpecification.toAccess;

	vkCmdPipelineBarrier(cmdBuffer, barrierSpecification.fromStage, barrierSpecification.toStage, 
		0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
}

void VulkanUtils::imageMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanImage>& image, MemoryBarrierSpecification barrierSpecification)
{
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemoryBarrier.image = image->getVkImage();
	imageMemoryBarrier.subresourceRange.aspectMask = image->getSpecification().Aspect;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;

	VulkanImageSpecification imgSpecification = image->getSpecification();
	imageMemoryBarrier.subresourceRange.layerCount = imgSpecification.LayerCount;
	imageMemoryBarrier.subresourceRange.levelCount = imgSpecification.Mips;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.srcAccessMask = barrierSpecification.fromAccess;
	imageMemoryBarrier.dstAccessMask = barrierSpecification.toAccess;

	vkCmdPipelineBarrier(cmdBuffer, barrierSpecification.fromStage, barrierSpecification.toStage,
		0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}
