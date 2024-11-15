#include "VulkanComputePipeline.h"

#include "VulkanDevice.h"

#include <cassert>

VulkanComputePipeline::VulkanComputePipeline(const std::shared_ptr<VulkanShader>& shader, uint32_t pushConstantSize)
{
	VkDevice device = VulkanDevice::getVulkanDevice();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = (uint32_t)shader->getDescriptorSetLayouts().size();
	pipelineLayoutInfo.pSetLayouts = shader->getDescriptorSetLayouts().data();

	VkPushConstantRange range{};
	range.offset = 0;
	range.size = pushConstantSize;
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	if (pushConstantSize != 0)
	{
		pipelineLayoutInfo.pPushConstantRanges = &range;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
	}

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
		assert(false);

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = m_PipelineLayout;
	pipelineInfo.stage = shader->getStageCreateInfo(ShaderStage::COMPUTE);

	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		assert(false);
}

VulkanComputePipeline::~VulkanComputePipeline()
{
	VkDevice device = VulkanDevice::getVulkanDevice();
	vkDestroyPipeline(device, m_Pipeline, nullptr);
	vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
}

void VulkanComputePipeline::bufferMemoryBarrier(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, const std::shared_ptr<VulkanBaseBuffer>& buffer, 
	VkAccessFlagBits from, VkAccessFlagBits to)
{
	bufferMemoryBarrier(cmdBuffer->getCurrentCommandBuffer(), buffer, from, to);
}

void VulkanComputePipeline::imageMemoryBarrier(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, const std::shared_ptr<VulkanImage>& image, 
	VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage)
{
	imageMemoryBarrier(cmdBuffer->getCurrentCommandBuffer(), image, from, fromStage, to, toStage);
}

void VulkanComputePipeline::bufferMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBaseBuffer>& buffer, VkAccessFlagBits from, VkAccessFlagBits to)
{
	VkBufferMemoryBarrier bufferMemoryBarrier = {};
	bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemoryBarrier.buffer = buffer->getBuffer();
	bufferMemoryBarrier.offset = 0;
	bufferMemoryBarrier.size = VK_WHOLE_SIZE;
	bufferMemoryBarrier.srcAccessMask = from;
	bufferMemoryBarrier.dstAccessMask = to;
	vkCmdPipelineBarrier(cmdBuffer, from, to, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
}

void VulkanComputePipeline::imageMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanImage>& image, VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage)
{
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemoryBarrier.image = image->getVkImage();
	imageMemoryBarrier.subresourceRange.aspectMask = image->getSpecification().Aspect;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.srcAccessMask = from;
	imageMemoryBarrier.dstAccessMask = to;
	vkCmdPipelineBarrier(cmdBuffer, fromStage, toStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}
