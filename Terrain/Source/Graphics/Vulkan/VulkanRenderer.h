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

	static void beginRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
		const std::shared_ptr<RenderPass>& renderPass);
	static void endRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer);

	static void beginSwapchainRenderPass(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, const std::shared_ptr<RenderPass>& renderpass);

	static void preparePipeline(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer,
		const std::shared_ptr<RenderPass>& renderPass);

	static void preparePipeline(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer,
		const std::shared_ptr<RenderPass>& renderPass, uint32_t descriptorSetIndex);

	static void dispatchCompute(const std::shared_ptr<VulkanRenderCommandBuffer>& commandBuffer, 
		const std::shared_ptr<VulkanComputePass>& computePass, glm::ivec3 workgroups, uint32_t pushConstantSize = 0, void* data = nullptr);

	static void dispatchCompute(VkCommandBuffer commandBuffer,
		const std::shared_ptr<VulkanComputePass>& computePass, glm::ivec3 workgroups, uint32_t pushConstantSize = 0, void* data = nullptr);

	static void dispatchCompute(VkCommandBuffer commandBuffer,
		const std::shared_ptr<VulkanComputePipeline>& computePipeline, const std::shared_ptr<VulkanDescriptorSet>& descriptorSet,
		glm::ivec3 workgroups, uint32_t pushConstantSize = 0, void* data = nullptr);

private:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;

private:
	static std::shared_ptr<VulkanSwapchain> m_Swapchain;
};