#pragma once

#include "Core/Window.h"

#include <vulkan/vulkan.h>
#include <memory>

class VulkanImgui
{
public:
	VulkanImgui() = default;
	~VulkanImgui() = default;

	void Initialize(const std::shared_ptr<Window>& window);
	void Destroy();

	void beginFrame();
	void endFrame();

private:
	VkDescriptorPool m_DescriptorPool;
};
