#pragma once

#include "Core/Window.h"
#include "VulkanSwapchain.h"

#include <vulkan/vulkan.h>
#include <memory>

class VulkanImgui
{
public:
	VulkanImgui() = default;
	~VulkanImgui() = default;

	void Initialize(const std::shared_ptr<Window>& window, const std::shared_ptr<VulkanSwapchain>& swapchain);
	void Destroy();

	void beginFrame();
	void endFrame(VkCommandBuffer commandBuffer);

private:
	VkDescriptorPool m_DescriptorPool;
};
