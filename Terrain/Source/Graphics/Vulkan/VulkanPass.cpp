#include "VulkanPass.h"
#include "VulkanDevice.h"

void VulkanRenderPass::Prepare(VkCommandBuffer commandBuffer, uint32_t descSetIndex, uint32_t pushConstantSize, void* data, VkShaderStageFlagBits pcFlags)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = float(VulkanSwapchain::Extent.width);
	viewport.height = float(VulkanSwapchain::Extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	float lineWidth = Pipeline->getSpecification().resterizationSpecification.lineWidth;
	vkCmdSetLineWidth(commandBuffer, lineWidth);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = VulkanSwapchain::Extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->getVkPipeline());

	if (data)
		vkCmdPushConstants(commandBuffer, Pipeline->getVkPipelineLayout(), pcFlags, 0,
			pushConstantSize, data);

	if (DescriptorSet != nullptr)
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->getVkPipelineLayout(),
			0, DescriptorSet->getNumberOfSets(),
			DescriptorSet->getDescriptorSet(descSetIndex).data(), 0, nullptr);
}

void VulkanRenderPass::Begin(VkCommandBuffer commandBuffer)
{
	std::shared_ptr<VulkanFramebuffer> framebuffer = Pipeline->getTargetFramebuffer();

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = framebuffer->getRenderPass();
	renderPassInfo.framebuffer = framebuffer->getVkFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VulkanSwapchain::Extent;

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

void VulkanRenderPass::End(VkCommandBuffer commandBuffer)
{
	vkCmdEndRenderPass(commandBuffer);
}

void VulkanComputePass::Prepare(VkCommandBuffer commandBuffer, uint32_t descSetIndex, uint32_t pushConstantSize, void* data)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->getVkPipeline());

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->getVkPipelineLayout(),
		0, DescriptorSet->getNumberOfSets(),
		DescriptorSet->getDescriptorSet(descSetIndex).data(), 0, nullptr);

	if (pushConstantSize != 0)
		vkCmdPushConstants(commandBuffer, Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
			pushConstantSize, data);
}

void VulkanComputePass::Dispatch(VkCommandBuffer commandBuffer, glm::ivec3 workgroups)
{
	vkCmdDispatch(commandBuffer, workgroups.x, workgroups.y, workgroups.z);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getComputeQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
		assert(false);
}

void VulkanSwapchainPass::Prepare(VkCommandBuffer commandBuffer, uint32_t descSetIndex, uint32_t pushConstantSize, void* data)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = float(VulkanSwapchain::Extent.width);
	viewport.height = float(VulkanSwapchain::Extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	float lineWidth = Pipeline->getSpecification().resterizationSpecification.lineWidth;
	vkCmdSetLineWidth(commandBuffer, lineWidth);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = VulkanSwapchain::Extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->getVkPipeline());

	if (DescriptorSet != nullptr)
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->getVkPipelineLayout(),
			0, DescriptorSet->getNumberOfSets(),
			DescriptorSet->getDescriptorSet(descSetIndex).data(), 0, nullptr);
}

void VulkanSwapchainPass::Begin(VkCommandBuffer commandBuffer)
{
	VkExtent2D extent = VulkanSwapchain::Extent;

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	renderPassInfo.renderPass = Swapchain->getRenderPass();
	renderPassInfo.framebuffer = Swapchain->getCurrentFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;

	VkClearValue clearValue;
	clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderPassInfo.pClearValues = &clearValue;
	renderPassInfo.clearValueCount = 1;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanSwapchainPass::End(VkCommandBuffer commandBuffer)
{
	vkCmdEndRenderPass(commandBuffer);
}
