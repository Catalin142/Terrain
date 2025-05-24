#pragma once

#include "VulkanDescriptorSet.h"
#include "VulkanRenderPipeline.h"
#include "VulkanComputePipeline.h"
#include "VulkanRenderCommandBuffer.h"

struct VulkanComputePass
{
	std::shared_ptr<VulkanComputePipeline> Pipeline;
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;

	void Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize = 0, void* data = nullptr, uint32_t descSetIndex);
	void Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize = 0, void* data = nullptr);

	void Begin(VkCommandBuffer commandBuffer);
	void End(VkCommandBuffer commandBuffer);
};

struct VulkanRenderPass
{
	std::shared_ptr<VulkanRenderPipeline> Pipeline;
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;

	void Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize = 0, void* data = nullptr, uint32_t descSetIndex);
	void Prepare(VkCommandBuffer commandBuffer, uint32_t pushConstantSize = 0, void* data = nullptr);

	void Dispatch(VkCommandBuffer commandBuffer, glm::ivec3 workgroups);
};

