#include "VulkanRenderer.h"

#include "VulkanDevice.h"

std::shared_ptr<VulkanSwapchain> VulkanRenderer::m_Swapchain = nullptr;

void VulkanRenderer::Initialize(const std::shared_ptr<VulkanSwapchain> swapchain)
{
	m_Swapchain = swapchain;
}

void VulkanRenderer::Destroy()
{
	m_Swapchain.reset();
}

VkRenderPass VulkanRenderer::getSwapchainRenderPass()
{
	return m_Swapchain->getRenderPass();
}

VkCommandBuffer VulkanRenderer::getSwapchainCurrentCommandBuffer()
{ 
	return m_Swapchain->getCurrentCommandBuffer();
}

uint32_t VulkanRenderer::getSwapchainWidth()
{
	return m_Swapchain->getWidth();
}

uint32_t VulkanRenderer::getSwapchainHeight()
{
	return m_Swapchain->getHeight();
}

uint32_t VulkanRenderer::getFramesInFlight()
{
	return m_Swapchain->getFramesInFlight();
}

uint32_t VulkanRenderer::getCurrentFrame()
{
	return m_Swapchain->getCurrentFrame();
}

void VulkanRenderer::beginRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
	const std::shared_ptr<VulkanRenderPass>& renderPass)
{
	VkCommandBuffer VulkanCommandBuffer = commandBuffer->getCurrentCommandBuffer();

	VkExtent2D extent = { m_Swapchain->getWidth(), m_Swapchain->getHeight() };

	std::shared_ptr<VulkanFramebuffer> framebuffer = renderPass->getTargetFramebuffer();

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
	clearValues.back().depthStencil = {framebuffer->getSpecification().depthClear, 0};
	renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(VulkanCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = float(extent.width);
	viewport.height = float(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(VulkanCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(VulkanCommandBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(VulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->getVulkanPipelineHandle());

	if (renderPass->hasDescriptorSet())
		vkCmdBindDescriptorSets(VulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->getVulkanPipelineLayout(),
			0, renderPass->getDescriptorSet()->getNumberOfSets(),
			renderPass->getDescriptorSet()->getDescriptorSet(m_Swapchain->getCurrentFrame()).data(), 0, nullptr);

}

void VulkanRenderer::endRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer)
{
	VkCommandBuffer vulkanCommandBuffer = commandBuffer->getCurrentCommandBuffer();
	vkCmdEndRenderPass(vulkanCommandBuffer);
}

void VulkanRenderer::beginSwapchainRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
	const std::shared_ptr<VulkanRenderPass>& renderPass)
{
	VkCommandBuffer VulkanCommandBuffer = commandBuffer->getCurrentCommandBuffer();

	VkExtent2D extent = { m_Swapchain->getWidth(), m_Swapchain->getHeight() };

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	// This is where I set up the swapchain target renderpass
	renderPassInfo.renderPass = m_Swapchain->getRenderPass();
	renderPassInfo.framebuffer = m_Swapchain->getCurrentFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;

	VkClearValue clearValue;
	clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderPassInfo.pClearValues = &clearValue;
	renderPassInfo.clearValueCount = 1;

	vkCmdBeginRenderPass(VulkanCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = float(extent.width);
	viewport.height = float(extent.height);
	vkCmdSetViewport(VulkanCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(VulkanCommandBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(VulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->getVulkanPipelineHandle());

	if (renderPass->hasDescriptorSet())
		vkCmdBindDescriptorSets(VulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->getVulkanPipelineLayout(),
			0, renderPass->getDescriptorSet()->getNumberOfSets(),
			renderPass->getDescriptorSet()->getDescriptorSet(m_Swapchain->getCurrentFrame()).data(), 0, nullptr);
}

void VulkanRenderer::dispatchCompute(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer,
	const std::shared_ptr<VulkanComputePipeline>& computePipeline, glm::ivec3 workgroups)
{
	vkCmdDispatch(commandBuffer->getCurrentCommandBuffer(), workgroups.x, workgroups.y, workgroups.z);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getComputeQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
		assert(false);
}

