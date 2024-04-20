#include "VulkanPipeline.h"

#include "VulkanRenderer.h"
#include "VulkanDevice.h"

#include <cassert>

VkPrimitiveTopology getVulkanTopology(PrimitiveTopology topology)
{
	switch (topology)
	{
	case PrimitiveTopology::POINTS:		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case PrimitiveTopology::LINES:		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PrimitiveTopology::TRIANGLES:	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	default: assert(false);
	}

	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

VkCompareOp getVulkanDepthCompareOperation(DepthCompare comp)
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

VulkanPipeline::VulkanPipeline(const PipelineSpecification& spec) : m_Specification(spec)
{
	Recreate();
}

VulkanPipeline::~VulkanPipeline()
{
	VkDevice device = VulkanDevice::getVulkanDevice();
	vkDestroyPipeline(device, m_Pipeline, nullptr);
	vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
}

void VulkanPipeline::Recreate()
{
	if (m_Pipeline)
	{
		VkDevice device = VulkanDevice::getVulkanDevice();
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	// dynamic state
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	if (m_Specification.lineWidth != 1.0f)
		dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);


	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();


	// vertex input
	VkVertexInputBindingDescription bindingDescription = m_Specification.vertexBufferLayout.getBindindDescription();
	const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions =
		m_Specification.vertexBufferLayout.getInputAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	if (m_Specification.vertexBufferLayout.hasInput())
	{
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	}

	// primitive topology
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = getVulkanTopology(m_Specification.Topology);
	inputAssembly.primitiveRestartEnable = VK_FALSE;


	// viewport
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;


	// restorizor
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = m_Specification.Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = m_Specification.Culling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	rasterizer.lineWidth = m_Specification.lineWidth;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// multisampling
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_TRUE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	if (m_Specification.Framebuffer)
		multisampling.rasterizationSamples = (VkSampleCountFlagBits)m_Specification.Framebuffer->getSpecification().Samples;

	uint32_t attachmentCount = 1;
	std::vector<FramebufferAttachment> fbAttachments;

	if (m_Specification.Framebuffer)
	{
		attachmentCount = (uint32_t)m_Specification.Framebuffer->getSpecification().Attachments.size();
		fbAttachments = m_Specification.Framebuffer->getSpecification().Attachments;
	}

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendStates;
	for (uint32_t i = 0; i < attachmentCount; i++)
	{
		colorBlendStates.push_back(VkPipelineColorBlendAttachmentState{});
		VkPipelineColorBlendAttachmentState& colorBlendAttachment = colorBlendStates.back();
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

		if (!m_Specification.Framebuffer)
			continue;

		switch (fbAttachments[i].blendMode)
		{
		case BlendMode::SRC_ALPHA_ONE_MINUS_SRC_ALPHA:
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case BlendMode::ONE_ZERO:
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			break;
		case BlendMode::ZERO_SRC_COLOR:
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			break;

		default:
			assert(false);
		}
	}
	// color blending
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = (uint32_t)colorBlendStates.size();
	colorBlending.pAttachments = colorBlendStates.data();

	bool hasDepth = false;
	if (m_Specification.Framebuffer)
		hasDepth = m_Specification.Framebuffer->getSpecification().DepthAttachment.Format != VK_FORMAT_UNDEFINED;


	// depth stencil
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.depthTestEnable = hasDepth ? VK_TRUE : VK_FALSE;
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = m_Specification.depthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = m_Specification.depthWrite ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = getVulkanDepthCompareOperation(m_Specification.depthCompareFunction);
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// layout create info
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	const std::vector<VkDescriptorSetLayout>& descSetLayout = m_Specification.Shader->getDescriptorSetLayouts();
	pipelineLayoutInfo.setLayoutCount = (uint32_t)descSetLayout.size();
	pipelineLayoutInfo.pSetLayouts = descSetLayout.data();

	std::vector<VkPushConstantRange> pushConstantRanges;
	for (const PushConstant& pc : m_Specification.pushConstants)
	{
		VkPushConstantRange& currentRange = pushConstantRanges.emplace_back();
		currentRange.offset = 0;
		currentRange.size = pc.Size;
		currentRange.stageFlags = pc.Stage;
	}

	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
	pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();

	if (vkCreatePipelineLayout(VulkanDevice::getVulkanDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
		assert(false);

	ShaderStage stages[] = { ShaderStage::VERTEX, ShaderStage::GEOMETRY, ShaderStage::FRAGMENT };

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	for (uint32_t i = 0; i < 3; i++)
		if (m_Specification.Shader->hasStage(stages[i]))
			shaderStages.push_back(m_Specification.Shader->getStageCreateInfo(stages[i]));

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();;
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_PipelineLayout;
	pipelineInfo.renderPass = VulkanRenderer::getSwapchainRenderPass();
	if (m_Specification.Framebuffer)
		pipelineInfo.renderPass = m_Specification.Framebuffer->getRenderPass();

	if (vkCreateGraphicsPipelines(VulkanDevice::getVulkanDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		assert(false);
}
