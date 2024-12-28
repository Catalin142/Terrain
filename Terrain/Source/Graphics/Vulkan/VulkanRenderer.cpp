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
	const std::shared_ptr<RenderPass>& renderPass)
{
	VkCommandBuffer VulkanCommandBuffer = commandBuffer->getCurrentCommandBuffer();

	VkExtent2D extent = { m_Swapchain->getWidth(), m_Swapchain->getHeight() };

	std::shared_ptr<VulkanFramebuffer> framebuffer = renderPass->Pipeline->getTargetFramebuffer();

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
}

void VulkanRenderer::endRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer)
{
	VkCommandBuffer vulkanCommandBuffer = commandBuffer->getCurrentCommandBuffer();
	vkCmdEndRenderPass(vulkanCommandBuffer);
}

void VulkanRenderer::beginSwapchainRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
	const std::shared_ptr<RenderPass>& renderPass)
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
}

void VulkanRenderer::preparePipeline(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, const std::shared_ptr<RenderPass>& renderPass)
{
	VkCommandBuffer VulkanCommandBuffer = commandBuffer->getCurrentCommandBuffer();

	VkExtent2D extent = { m_Swapchain->getWidth(), m_Swapchain->getHeight() };

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = float(extent.width);
	viewport.height = float(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(VulkanCommandBuffer, 0, 1, &viewport);

	float lineWidth = renderPass->Pipeline->getSpecification().lineWidth;

	if (renderPass->Pipeline->getSpecification().lineWidth != 1.0f)
		vkCmdSetLineWidth(VulkanCommandBuffer, renderPass->Pipeline->getSpecification().lineWidth);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(VulkanCommandBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(VulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->Pipeline->getVkPipeline());

	if (renderPass->DescriptorSet != nullptr)
		vkCmdBindDescriptorSets(VulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass->Pipeline->getVkPipelineLayout(),
			0, renderPass->DescriptorSet->getNumberOfSets(),
			renderPass->DescriptorSet->getDescriptorSet(m_Swapchain->getCurrentFrame()).data(), 0, nullptr);
}

static std::mutex queueMutex;

void VulkanRenderer::dispatchCompute(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, const std::shared_ptr<VulkanComputePass>& computePass, glm::ivec3 workgroups,
	uint32_t pushConstantSize, void* data)
{
	dispatchCompute(commandBuffer->getCurrentCommandBuffer(), computePass, workgroups, pushConstantSize, data);
}

void VulkanRenderer::dispatchCompute(VkCommandBuffer cmd, const std::shared_ptr<VulkanComputePass>& computePass, glm::ivec3 workgroups, uint32_t pushConstantSize, void* data)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePass->Pipeline->getVkPipeline());

	if (computePass->DescriptorSet != nullptr)
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePass->Pipeline->getVkPipelineLayout(),
			0, computePass->DescriptorSet->getNumberOfSets(),
			computePass->DescriptorSet->getDescriptorSet(VulkanRenderer::getCurrentFrame()).data(), 0, nullptr);

	if (pushConstantSize != 0)
		vkCmdPushConstants(cmd, computePass->Pipeline->getVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
			pushConstantSize, data);

	vkCmdDispatch(cmd, workgroups.x, workgroups.y, workgroups.z);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	{
		std::lock_guard<std::mutex> lock(queueMutex);
		if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getComputeQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
			assert(false);
	}
}

void VulkanRenderer::dispatchCompute(VkCommandBuffer commandBuffer, 
	const std::shared_ptr<VulkanComputePipeline>& computePipeline, const std::shared_ptr<VulkanDescriptorSet>& descriptorSet, 
	glm::ivec3 workgroups, uint32_t pushConstantSize, void* data)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->getVkPipeline());

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->getVkPipelineLayout(),
		0, descriptorSet->getNumberOfSets(),
		descriptorSet->getDescriptorSet(VulkanRenderer::getCurrentFrame()).data(), 0, nullptr);

	if (pushConstantSize != 0)
		vkCmdPushConstants(commandBuffer, computePipeline->getVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
			pushConstantSize, data);

	vkCmdDispatch(commandBuffer, workgroups.x, workgroups.y, workgroups.z);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	{
		std::lock_guard<std::mutex> lock(queueMutex);
		if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getComputeQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
			assert(false);
	}
}

