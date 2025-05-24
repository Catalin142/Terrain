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

	VkPipeline getVkPipeline() { return m_Pipeline; }
	VkPipelineLayout getVkPipelineLayout() { return m_PipelineLayout; }

private:
	VkPipeline m_Pipeline;
	VkPipelineLayout m_PipelineLayout;
};
