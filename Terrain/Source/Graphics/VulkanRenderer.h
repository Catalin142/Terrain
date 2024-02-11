#pragma once
#include <memory>

#include "VulkanSwapchain.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanComputePipeline.h"
#include "VulkanRenderPass.h"

class VulkanRenderer
{
	friend class VulkanSwapchain;

public:
	static void Initialize(const std::shared_ptr<VulkanSwapchain> swapchain);
	static void Destroy();

	static VkRenderPass getSwapchainRenderPass();
	static VkCommandBuffer getSwapchainCurrentCommandBuffer();

	static uint32_t getSwapchainWidth();
	static uint32_t getSwapchainHeight();
	static uint32_t getFramesInFlight();
	static uint32_t getCurrentFrame();

	static void beginRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
		const std::shared_ptr<VulkanRenderPass>& renderPass);
	static void endRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer);

	static void beginSwapchainRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, const std::shared_ptr<VulkanRenderPass>& renderpass);

	static void dispatchCompute(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
		const std::shared_ptr<VulkanComputePipeline>& computePipeline, glm::ivec3 workgroups);

private:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;

private:
	static std::shared_ptr<VulkanSwapchain> m_Swapchain;
};