#pragma once

#include "VulkanDescriptorSet.h"
#include "VulkanPipeline.h"
#include "VulkanImage.h"

#include <vulkan/vulkan.h>
#include <vector>

struct VulkanRenderPass
{
	VulkanRenderPass() = default;

	void setPipeline(const std::shared_ptr<VulkanPipeline>& pipeline) { m_Pipeline = pipeline; }
	void setDescriptorSet(const std::shared_ptr<VulkanDescriptorSet>& descSet) { m_DescriptorSet = descSet; }

	void Resize(uint32_t width, uint32_t height);

	std::shared_ptr<VulkanFramebuffer> getTargetFramebuffer();
	std::shared_ptr<VulkanImage> getOutput(uint32_t index);
	std::shared_ptr<VulkanImage> getDepthOutput();

	bool hasDescriptorSet() { return m_DescriptorSet != nullptr; }

	VkPipeline getVulkanPipelineHandle() { return m_Pipeline->getVkPipeline(); }
	VkPipelineLayout getVulkanPipelineLayout() { return m_Pipeline->getVkPipelineLayout(); }
	std::shared_ptr<VulkanDescriptorSet> getDescriptorSet() { return m_DescriptorSet; }

private:
	std::shared_ptr<VulkanDescriptorSet> m_DescriptorSet = nullptr;
	std::shared_ptr<VulkanPipeline> m_Pipeline;
};
