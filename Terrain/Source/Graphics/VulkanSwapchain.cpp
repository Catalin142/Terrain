#include "VulkanSwapchain.h"

#include "VulkanDevice.h"

#include <iostream>
#include <algorithm>
#include <cassert>

VulkanSwapchain::~VulkanSwapchain()
{
	Destroy();
}

void VulkanSwapchain::Initialize()
{
	VulkanDevice* Device = VulkanDevice::getVulkanContext();
	VkDevice logicalDevice = Device->getLogicalDevice();

	SwapchainSupportDetails swapChainSupport = Device->getSwapchainCapabilities();

	m_ImageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (swapChainSupport.capabilities.maxImageCount > 0 && m_ImageCount > swapChainSupport.capabilities.maxImageCount)
		m_ImageCount = swapChainSupport.capabilities.maxImageCount;

	m_SurfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
	m_SurfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	m_PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

	////////////////////////////////////////////////////////////////////////////////////////
	// Render pass
	////////////////////////////////////////////////////////////////////////////////////////

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
		assert(false);

	////////////////////////////////////////////////////////////////////////////////////////
	// Command buffer
	////////////////////////////////////////////////////////////////////////////////////////

	m_FramesInFlight = 1;

	m_CommandBuffers.resize(m_FramesInFlight);

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	for (SwapchainCommandBuffer& commandBuffer : m_CommandBuffers)
	{
		if (vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &commandBuffer.commandPool))
			throw(false);

		commandBufferAllocateInfo.commandPool = commandBuffer.commandPool;
		if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &commandBuffer.commandBuffer) != VK_SUCCESS)
			throw(false);
	}
	////////////////////////////////////////////////////////////////////////////////////////
	// Sync objects
	////////////////////////////////////////////////////////////////////////////////////////

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // l face in signaled state

	m_inFlightFences.resize(m_FramesInFlight);
	for (uint32_t i = 0; i < m_FramesInFlight; i++)
		if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_inFlightFences[i])) throw(false);
}

void VulkanSwapchain::Create(uint32_t width, uint32_t height)
{
	VkSwapchainKHR oldSwapchain = m_SwapChain;

	VulkanDevice* Device = VulkanDevice::getVulkanContext();
	VkDevice logicalDevice = Device->getLogicalDevice();

	SwapchainSupportDetails swapChainSupport = Device->getSwapchainCapabilities();

	m_Width	= std::clamp(width, swapChainSupport.capabilities.minImageExtent.width, swapChainSupport.capabilities.maxImageExtent.width);
	m_Height= std::clamp(height, swapChainSupport.capabilities.minImageExtent.height, swapChainSupport.capabilities.maxImageExtent.height);

	VkSurfaceFormatKHR surfaceFormat;
	surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
	surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	m_Extent = VkExtent2D{ m_Width, m_Height };

	VkSwapchainCreateInfoKHR swapChainInfo{};
	swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainInfo.surface = Device->getWindowSurface();
	swapChainInfo.minImageCount = m_ImageCount;
	swapChainInfo.imageFormat = surfaceFormat.format;
	swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainInfo.imageExtent = m_Extent;
	swapChainInfo.imageArrayLayers = 1;
	swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { Device->getGraphicsFamilyIndex(), Device->getPresentFamilyIndex() };
	if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
		swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // imaginile pot fi folosite intre mai multe queueri fara sa fiu explicit despre ownership
		swapChainInfo.queueFamilyIndexCount = 2;
		swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else { // in cazul in care presentQueue ul si graphicsQueue ul sunt acelasi, atunci pot folosi exclusive, ca nu trb sa il postesc
		swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // ownershipul trebuie dat manual, best performance
		swapChainInfo.queueFamilyIndexCount = 0; // Optional
		swapChainInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	swapChainInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	swapChainInfo.presentMode = m_PresentMode;
	swapChainInfo.clipped = VK_TRUE;

	swapChainInfo.oldSwapchain = oldSwapchain;

	if (vkCreateSwapchainKHR(logicalDevice, &swapChainInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		throw (false);

	if (oldSwapchain)
		vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);

	uint32_t imageCount;
	vkGetSwapchainImagesKHR(logicalDevice, m_SwapChain, &imageCount, nullptr);
	std::vector<VkImage> images(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, m_SwapChain, &imageCount, images.data());

	////////////////////////////////////////////////////////////////////////////////////////
	// Image views and sync objects
	////////////////////////////////////////////////////////////////////////////////////////

	for (uint32_t i = 0; i < m_ColorImages.size(); i++)
		vkDestroyImageView(logicalDevice, m_ColorImages[i].ImageView, nullptr);
	m_ColorImages.clear();

	for (uint32_t i = 0; i < m_Semaphores.size(); i++)
	{
		vkDestroySemaphore(logicalDevice, m_Semaphores[i].imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(logicalDevice, m_Semaphores[i].renderFinishedSemaphore, nullptr);
	}
	m_Semaphores.clear();

	m_ColorImages.resize(imageCount);
	m_Semaphores.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		m_ColorImages[i].Image = images[i];

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_ColorImages[i].Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, &m_ColorImages[i].ImageView) != VK_SUCCESS)
			throw(false);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &m_Semaphores[i].imageAvailableSemaphore)) assert(false);
		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &m_Semaphores[i].renderFinishedSemaphore)) assert(false);
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Framebuffers
	////////////////////////////////////////////////////////////////////////////////////////

	for (uint32_t i = 0; i < m_Framebuffers.size(); i++)
		vkDestroyFramebuffer(logicalDevice, m_Framebuffers[i], nullptr);

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = m_RenderPass;
	framebufferCreateInfo.attachmentCount = 1;
	framebufferCreateInfo.width = m_Width;
	framebufferCreateInfo.height = m_Height;
	framebufferCreateInfo.layers = 1;

	m_Framebuffers.resize(m_ImageCount);
	for (uint32_t i = 0; i < m_Framebuffers.size(); i++)
	{
		framebufferCreateInfo.pAttachments = &m_ColorImages[i].ImageView;
		if(vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &m_Framebuffers[i])) assert(false);
	}
}

void VulkanSwapchain::onResize(uint32_t width, uint32_t height)
{
	VkDevice device = VulkanDevice::getVulkanDevice();
	vkDeviceWaitIdle(device);
	Create(width, height);
	vkDeviceWaitIdle(device);
}

void VulkanSwapchain::Destroy()
{
	auto device = VulkanDevice::getVulkanDevice();
	vkDeviceWaitIdle(device);

	if (m_SwapChain)
		vkDestroySwapchainKHR(VulkanDevice::getVulkanDevice(), m_SwapChain, nullptr);

	for (auto& image : m_ColorImages)
		vkDestroyImageView(device, image.ImageView, nullptr);

	if (m_RenderPass)
		vkDestroyRenderPass(device, m_RenderPass, nullptr);

	for (auto framebuffer : m_Framebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);

	for (uint32_t i = 0; i < m_CommandBuffers.size(); i++)
		vkDestroyCommandPool(device, m_CommandBuffers[i].commandPool, nullptr);

	for (uint32_t i = 0; i < m_Semaphores.size(); i++)
	{
		vkDestroySemaphore(device, m_Semaphores[i].imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, m_Semaphores[i].renderFinishedSemaphore, nullptr);
	}

	for (uint32_t i = 0; i < m_inFlightFences.size(); i++)
		vkDestroyFence(device, m_inFlightFences[i], nullptr);

	vkDeviceWaitIdle(device);
}

void VulkanSwapchain::beginFrame()
{
	VkDevice device = VulkanDevice::getVulkanDevice();

	VkResult res = vkAcquireNextImageKHR(device, m_SwapChain, UINT64_MAX, getImageAvailableSemaphore(m_currentFrameIndex), VK_NULL_HANDLE, &m_ImageIndex);

	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		onResize(m_Width, m_Height);
		return;
	}
	else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		throw(false);
}

void VulkanSwapchain::endFrame()
{
	VkCommandBuffer commandBuffer = getCommandBuffer(m_currentFrameIndex);

	VkDevice device = VulkanDevice::getVulkanDevice();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	VkSemaphore waitSemaphores[] = { getImageAvailableSemaphore(m_currentFrameIndex) };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	VkSemaphore signalSemaphores[] = { getRenderFinishedSemaphore(m_currentFrameIndex) };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	
	vkResetFences(device, 1, &m_inFlightFences[m_currentFrameIndex]);
	if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrameIndex]) != VK_SUCCESS)
		throw(false);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { m_SwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_ImageIndex;
	presentInfo.pResults = nullptr; // Optional
	
	VkResult res = vkQueuePresentKHR(VulkanDevice::getVulkanContext()->getPresentQueue(), &presentInfo);
	
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		onResize(m_Width, m_Height);
	}
	else if (res != VK_SUCCESS)
		throw(false);

	m_currentFrameIndex = (m_currentFrameIndex + 1) % m_FramesInFlight;

	vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrameIndex], VK_TRUE, UINT64_MAX);
}
