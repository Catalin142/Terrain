#include "VulkanSwapchain.h"

#include "VulkanDevice.h"
#include "Core/Instrumentor.h"

#include <iostream>
#include <algorithm>
#include <cassert>
#include <Terrain/VirtualMap/DynamicVirtualTerrainDeserializer.h>

VulkanSwapchain::~VulkanSwapchain()
{
	Destroy();
}

void VulkanSwapchain::Initialize()
{
	VulkanDevice* Device = VulkanDevice::getVulkanContext();
	VkDevice logicalDevice = Device->getLogicalDevice();

	VkSurfaceCapabilitiesKHR swapchainCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device->getPhysicalDevice(), Device->getWindowSurface(), &swapchainCapabilities);

	m_ImageCount = swapchainCapabilities.minImageCount + 1;

	if (swapchainCapabilities.maxImageCount > 0 && m_ImageCount > swapchainCapabilities.maxImageCount)
		m_ImageCount = swapchainCapabilities.maxImageCount;

	m_SurfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
	m_SurfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	m_PresentMode = m_vSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

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

	framesInFlight = m_vSync ? 2 : 1;

	////////////////////////////////////////////////////////////////////////////////////////
	// Sync objects
	////////////////////////////////////////////////////////////////////////////////////////

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // l face in signaled state

	m_inFlightFences.resize(framesInFlight);
	for (uint32_t i = 0; i < framesInFlight; i++)
		if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_inFlightFences[i])) assert(false);
}

void VulkanSwapchain::Create(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;

	VkSwapchainKHR oldSwapchain = m_SwapChain;

	VulkanDevice* Device = VulkanDevice::getVulkanContext();
	VkDevice logicalDevice = Device->getLogicalDevice();

	VkSurfaceCapabilitiesKHR swapchainCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device->getPhysicalDevice(), Device->getWindowSurface(), &swapchainCapabilities);

	m_Width	= std::clamp(width, swapchainCapabilities.minImageExtent.width, swapchainCapabilities.maxImageExtent.width);
	m_Height = std::clamp(height, swapchainCapabilities.minImageExtent.height, swapchainCapabilities.maxImageExtent.height);

	if (m_Width == 0 || m_Height == 0)
		return;

	VkSurfaceFormatKHR surfaceFormat;
	surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
	surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	Extent = VkExtent2D{ m_Width, m_Height };

	VkSwapchainCreateInfoKHR swapChainInfo{};
	swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainInfo.surface = Device->getWindowSurface();
	swapChainInfo.minImageCount = m_ImageCount;
	swapChainInfo.imageFormat = surfaceFormat.format;
	swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainInfo.imageExtent = Extent;
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

	swapChainInfo.preTransform = swapchainCapabilities.currentTransform;
	swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	swapChainInfo.presentMode = m_PresentMode;
	swapChainInfo.clipped = VK_TRUE;

	swapChainInfo.oldSwapchain = oldSwapchain;

	if (vkCreateSwapchainKHR(logicalDevice, &swapChainInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		assert(false);

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
			assert(false);

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

	VkResult res = vkAcquireNextImageKHR(device, m_SwapChain, UINT64_MAX, m_Semaphores[currentFrameIndex].imageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);
	
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		onResize(m_Width, m_Height);
		return;
	}
	else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		assert(false);
}

void VulkanSwapchain::endFrame(VkCommandBuffer commandBuffer)
{
	VkDevice device = VulkanDevice::getVulkanDevice();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	VkSemaphore waitSemaphores[] = { m_Semaphores[currentFrameIndex].imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	VkSemaphore signalSemaphores[] = { m_Semaphores[currentFrameIndex].renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &m_inFlightFences[currentFrameIndex]);

	{
		if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[currentFrameIndex]) != VK_SUCCESS)
			assert(false);
	}
}

void VulkanSwapchain::presentFrame()
{
	VkDevice device = VulkanDevice::getVulkanDevice();

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	VkSemaphore signalSemaphores[] = { m_Semaphores[currentFrameIndex].renderFinishedSemaphore };
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { m_SwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_ImageIndex;

	VkResult res = VK_SUCCESS;
	
	{
		res = vkQueuePresentKHR(VulkanDevice::getVulkanContext()->getPresentQueue(), &presentInfo);

		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			onResize(m_Width, m_Height);
		}
		else if (res != VK_SUCCESS)
			assert(false);

		currentFrameIndex = (currentFrameIndex + 1) % framesInFlight;

		vkWaitForFences(device, 1, &m_inFlightFences[currentFrameIndex], VK_TRUE, UINT64_MAX);
	}
}

bool VulkanSwapchain::getCurrentFenceStatus(uint32_t index)
{
	return vkGetFenceStatus(VulkanDevice::getVulkanDevice(), m_inFlightFences[index]);
}
