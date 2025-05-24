#pragma once

#include "VulkanVertexBufferLayout.h"

#include <vulkan/vulkan.h>
#include <cassert>

enum class PrimitiveTopology
{
	POINTS,
	LINES,
	TRIANGLES,
	PATCHES,
};

static VkPrimitiveTopology getVulkanTopology(PrimitiveTopology topology)
{
	switch (topology)
	{
	case PrimitiveTopology::POINTS:		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case PrimitiveTopology::LINES:		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PrimitiveTopology::TRIANGLES:	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PrimitiveTopology::PATCHES:	return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	default: assert(false);
	}

	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

enum class DepthCompare
{
	NEVER,
	NOT_EQUAL,
	LESS,
	GRATER,
	EQUAL,
	ALWAYS,
};

static VkCompareOp getVulkanDepthCompareOperation(DepthCompare comp)
{
	switch (comp)
	{
	case DepthCompare::NEVER:		return VK_COMPARE_OP_NEVER;
	case DepthCompare::NOT_EQUAL:	return VK_COMPARE_OP_NOT_EQUAL;
	case DepthCompare::LESS:		return VK_COMPARE_OP_LESS;
	case DepthCompare::GRATER:		return VK_COMPARE_OP_GREATER;
	case DepthCompare::EQUAL:		return VK_COMPARE_OP_EQUAL;
	case DepthCompare::ALWAYS:		return VK_COMPARE_OP_ALWAYS;
	default: assert(false);
	}

	return VK_COMPARE_OP_MAX_ENUM;
}

struct ResterizationStateSpecification
{
	bool Culling = false;
	bool Wireframe = false;
	float lineWidth = 1.0f;
};

struct DepthStencilStateSpecification
{
	bool depthTest = true;
	bool depthWrite = true;
	DepthCompare depthCompareFunction = DepthCompare::NEVER;
};

// Specifies how data is laid out for the vertex shader, what topology
inline VkPipelineInputAssemblyStateCreateInfo createInputAssemblyState(PrimitiveTopology topology)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = getVulkanTopology(topology);
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	return inputAssembly;
}

// TODO: Implement if needed
// Sets the number of viewports and scrissors
inline VkPipelineViewportStateCreateInfo createViewportState(uint32_t viewportCount = 1, uint32_t scissorCount = 1)
{
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = viewportCount;
	viewportState.scissorCount = scissorCount;

	return viewportState;
}

inline VkPipelineRasterizationStateCreateInfo createResterizationState(ResterizationStateSpecification spec)
{
	VkPipelineRasterizationStateCreateInfo rasterizerState{};
	rasterizerState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerState.depthClampEnable = VK_FALSE;
	rasterizerState.rasterizerDiscardEnable = VK_FALSE;
	rasterizerState.polygonMode = spec.Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizerState.cullMode = spec.Culling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	rasterizerState.lineWidth = spec.lineWidth;
	rasterizerState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerState.depthBiasEnable = VK_FALSE;

	return rasterizerState;
}

inline VkPipelineMultisampleStateCreateInfo createMultisamplingState(uint32_t samples = 1)
{
	VkPipelineMultisampleStateCreateInfo multisamplingState{};
	multisamplingState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingState.sampleShadingEnable = VK_TRUE;
	multisamplingState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingState.rasterizationSamples = (VkSampleCountFlagBits)samples;

	return multisamplingState;
}

// Depth specification 
inline VkPipelineDepthStencilStateCreateInfo createDepthStencilState(DepthStencilStateSpecification spec)
{
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = spec.depthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = spec.depthWrite ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = getVulkanDepthCompareOperation(spec.depthCompareFunction);
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	return depthStencil;
}

inline VkPipelineTessellationStateCreateInfo createTessellationState(uint32_t patchControlPoints = 0)
{
	VkPipelineTessellationStateCreateInfo tessellationInfo{};
	tessellationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessellationInfo.patchControlPoints = patchControlPoints;

	return tessellationInfo;
}
