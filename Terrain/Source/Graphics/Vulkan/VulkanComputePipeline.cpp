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
