#pragma once

#include "VulkanDescriptorSet.h"
#include "VulkanPipeline.h"
#include "VulkanComputePipeline.h"
#include "VulkanRenderCommandBuffer.h"

struct VulkanComputePass
{
	std::shared_ptr<VulkanComputePipeline> Pipeline;
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
};

struct RenderPass
{
	std::shared_ptr<VulkanDescriptorSet> DescriptorSet;
	std::shared_ptr<VulkanPipeline> Pipeline;
};

