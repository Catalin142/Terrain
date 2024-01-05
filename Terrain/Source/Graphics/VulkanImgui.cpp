#include "VulkanImgui.h"

#include "VulkanDevice.h"
#include "VulkanRenderer.h"

#include <vector>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

void VulkanImgui::Initialize(const std::shared_ptr<Window>& window)
{
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	vkCreateDescriptorPool(VulkanDevice::getVulkanDevice(), &pool_info, nullptr, &m_DescriptorPool);

	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(window->getHandle(), true);

	VulkanDevice* Device = VulkanDevice::getVulkanContext();

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = Device->getInstance();
	init_info.PhysicalDevice = Device->getGPU();
	init_info.Device = Device->getLogicalDevice();
	init_info.Queue = Device->getGraphicsQueue();
	init_info.DescriptorPool = m_DescriptorPool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, VulkanRenderer::getSwapchainRenderPass());

	ImGui_ImplVulkan_CreateFontsTexture();
}

void VulkanImgui::Destroy()
{
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(VulkanDevice::getVulkanDevice(), m_DescriptorPool, nullptr);
}

void VulkanImgui::beginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();

	ImGui::NewFrame();
}

void VulkanImgui::endFrame()
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), VulkanRenderer::getSwapchainCurrentCommandBuffer());
	ImGui::EndFrame();
}
