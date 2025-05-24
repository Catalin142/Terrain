#pragma once
#include <memory>

#include "VulkanSwapchain.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanComputePipeline.h"
#include "VulkanPass.h"

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

	static void beginRenderPass(VkCommandBuffer commandBuffer, const VulkanRenderPass& renderPass);
	static void endRenderPass(VkCommandBuffer commandBuffer);

	static void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);

	static void preparePipeline(VkCommandBuffer commandBuffer, const VulkanRenderPass& renderPass);
	static void preparePipeline(VkCommandBuffer commandBuffer, const VulkanRenderPass& renderPass, uint32_t descriptorSetIndex);

	static void dispatchCompute(VkCommandBuffer commandBuffer,
		const VulkanComputePass& computePass, uint32_t descriptorSetUsed, glm::ivec3 workgroups, uint32_t pushConstantSize = 0, void* data = nullptr);
	static void dispatchCompute(VkCommandBuffer commandBuffer,
		const VulkanComputePass& computePass, glm::ivec3 workgroups, uint32_t pushConstantSize = 0, void* data = nullptr);

private:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;

private:
	inline static std::shared_ptr<VulkanSwapchain> m_Swapchain;
};