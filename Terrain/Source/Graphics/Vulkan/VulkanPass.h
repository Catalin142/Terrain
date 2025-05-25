#pragma once

#include "VulkanDescriptorSet.h"
#include "VulkanRenderPipeline.h"
#include "VulkanComputePipeline.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanSwapchain.h"

struct VulkanRenderPass
{
	std::shared_ptr<VulkanRenderPipeline> Pipeline;
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;

	void Prepare(VkCommandBuffer commandBuffer, uint32_t descSetIndex, uint32_t pushConstantSize = 0, void* data = nullptr, VkShaderStageFlagBits pcFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);

	void Begin(VkCommandBuffer commandBuffer);
	void End(VkCommandBuffer commandBuffer);
};

struct VulkanSwapchainPass
{
	std::shared_ptr<VulkanRenderPipeline> Pipeline;
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
	std::shared_ptr<VulkanSwapchain> Swapchain;

	void Prepare(VkCommandBuffer commandBuffer, uint32_t descSetIndex, uint32_t pushConstantSize = 0, void* data = nullptr);

	void Begin(VkCommandBuffer commandBuffer);
	void End(VkCommandBuffer commandBuffer);
};

struct VulkanComputePass
{
	std::shared_ptr<VulkanComputePipeline> Pipeline;
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;

	void Prepare(VkCommandBuffer commandBuffer, uint32_t descSetIndex, uint32_t pushConstantSize = 0, void* data = nullptr);
	void Dispatch(VkCommandBuffer commandBuffer, glm::ivec3 workgroups);
};

