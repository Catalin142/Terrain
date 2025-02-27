#pragma once

#include "VulkanShader.h"
#include "VulkanImage.h"
#include "VulkanBuffer.h"
#include "VulkanRenderCommandBuffer.h"

#include <memory>
#include <vulkan/vulkan.h>

class VulkanComputePipeline
{
public:
	VulkanComputePipeline(const std::shared_ptr<VulkanShader>& shader, uint32_t pushConstantSize = 0);
	~VulkanComputePipeline();

	static void bufferMemoryBarrier(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, const std::shared_ptr<VulkanBuffer>& buffer,
		VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage);
	static void imageMemoryBarrier(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, const std::shared_ptr<VulkanImage>& image,
		VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage, uint32_t mips = 1, uint32_t mip = 1);

	static void bufferMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& buffer,
		VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage);
	static void imageMemoryBarrier(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanImage>& image,
		VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage, uint32_t mips = 1, uint32_t mip = 1);

	VkPipeline getVkPipeline() { return m_Pipeline; }
	VkPipelineLayout getVkPipelineLayout() { return m_PipelineLayout; }

private:
	VkPipeline m_Pipeline;
	VkPipelineLayout m_PipelineLayout;
};
