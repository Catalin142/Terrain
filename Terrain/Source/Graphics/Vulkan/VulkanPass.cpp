#include "VulkanPass.h"

void VulkanRenderPass::Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize, void* data, uint32_t descSetIndex)
{
	VkExtent2D extent = { m_Swapchain->getWidth(), m_Swapchain->getHeight() };

	std::shared_ptr<VulkanFramebuffer> framebuffer = Pipeline->getTargetFramebuffer();

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = framebuffer->getRenderPass();
	renderPassInfo.framebuffer = framebuffer->getVkFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;

	std::vector<VkClearValue> clearValues;
	for (uint32_t i = 0; i < framebuffer->getSpecification().Attachments.size(); i++)
	{
		clearValues.emplace_back();
		clearValues.back().color = {
			framebuffer->getSpecification().colorClear.r,
			framebuffer->getSpecification().colorClear.g,
			framebuffer->getSpecification().colorClear.b,
			framebuffer->getSpecification().colorClear.a,
		};
	}

	clearValues.emplace_back();
	clearValues.back().depthStencil = { framebuffer->getSpecification().depthClear, 0 };
	renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderPass::Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize, void* data)
{
}

void VulkanRenderPass::Dispatch(VkCommandBuffer commandBuffer, glm::ivec3 workgroups)
{
}

void VulkanComputePass::Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize, void* data, uint32_t descSetIndex)
{
}

void VulkanComputePass::Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize, void* data)
{
}

void VulkanComputePass::Begin(VkCommandBuffer commandBuffer)
{
}

void VulkanComputePass::End(VkCommandBuffer commandBuffer)
{
}
