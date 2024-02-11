#pragma once

#include "VulkanShader.h"
#include "VulkanImage.h"
#include "VulkanBaseBuffer.h"
#include "VulkanRenderCommandBuffer.h"

#include <memory>
#include <vulkan/vulkan.h>

class VulkanComputePipeline
{
public:
	VulkanComputePipeline(const std::shared_ptr<VulkanShader>& shader);
	~VulkanComputePipeline();

	void bufferMemoryBarrier(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, const std::shared_ptr<VulkanBaseBuffer>& buffer, 
		VkAccessFlagBits from, VkAccessFlagBits to);
	void imageMemoryBarrier(const std::shared_ptr<VulkanRenderCommandBuffer>& cmdBuffer, const std::shared_ptr<VulkanImage>& image,
		VkAccessFlagBits from, VkPipelineStageFlags fromStage, VkAccessFlagBits to, VkPipelineStageFlags toStage);

	VkPipeline getVkPipeline() { return m_Pipeline; }
	VkPipelineLayout getVkPipelineLayout() { return m_PipelineLayout; }

private:
	VkPipeline m_Pipeline;
	VkPipelineLayout m_PipelineLayout;
};
