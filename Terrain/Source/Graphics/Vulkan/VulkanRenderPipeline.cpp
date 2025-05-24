#include "VulkanRenderPipeline.h"

#include "VulkanRenderer.h"
#include "VulkanDevice.h"

#include <cassert>

VulkanRenderPipeline::VulkanRenderPipeline(const RenderPipelineSpecification& spec) : m_Specification(spec)
{// dynamic state
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();

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

	// layout create info
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	const std::vector<VkDescriptorSetLayout>& descSetLayout = m_Specification.Shader->getDescriptorSetLayouts();
	pipelineLayoutInfo.setLayoutCount = (uint32_t)descSetLayout.size();
	pipelineLayoutInfo.pSetLayouts = descSetLayout.data();

	std::vector<VkPushConstantRange> pushConstantRanges;
	for (const PushConstantSpecification& pc : m_Specification.pushConstants)
	{
		VkPushConstantRange& currentRange = pushConstantRanges.emplace_back();
		currentRange.offset = 0;
		currentRange.size = pc.Size;
		currentRange.stageFlags = getStage(pc.Stage);
	}

	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
	pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();

	if (vkCreatePipelineLayout(VulkanDevice::getVulkanDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
		assert(false);

	ShaderStage stages[] = { ShaderStage::VERTEX, ShaderStage::TESSELLATION_CONTROL,
		ShaderStage::GEOMETRY, ShaderStage::TESSELLATION_EVALUATION, ShaderStage::FRAGMENT };

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	for (uint32_t i = 0; i < 5; i++)
		if (m_Specification.Shader->hasStage(stages[i]))
			shaderStages.push_back(m_Specification.Shader->getStageCreateInfo(stages[i]));

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();

	VkVertexInputBindingDescription bindingDescription = m_Specification.vertexBufferLayout.getBindindDescription();
	const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions =
		m_Specification.vertexBufferLayout.getInputAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputState{};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	if (m_Specification.vertexBufferLayout.hasInput())
	{
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &bindingDescription;
		vertexInputState.vertexAttributeDescriptionCount = uint32_t(attributeDescriptions.size());
		vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState	= createInputAssemblyState(m_Specification.Topology);
	VkPipelineTessellationStateCreateInfo tessellationState		= createTessellationState(m_Specification.tessellationPatchControlPoints);
	VkPipelineViewportStateCreateInfo viewportState				= createViewportState();
	VkPipelineRasterizationStateCreateInfo rasterizationState	= createResterizationState(m_Specification.resterizationSpecification);
	VkPipelineMultisampleStateCreateInfo multisampleState		= createMultisamplingState(m_Specification.MMSASampleCount);
	VkPipelineDepthStencilStateCreateInfo depthStencilState		= createDepthStencilState(m_Specification.depthStateSpecification);

	pipelineInfo.pVertexInputState		= &vertexInputState;
	pipelineInfo.pInputAssemblyState	= &inputAssemblyState;
	pipelineInfo.pViewportState			= &viewportState;
	pipelineInfo.pRasterizationState	= &rasterizationState;
	pipelineInfo.pTessellationState		= &tessellationState;
	pipelineInfo.pMultisampleState		= &multisampleState;
	pipelineInfo.pDepthStencilState		= &depthStencilState;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicState;

	pipelineInfo.layout = m_PipelineLayout;
	pipelineInfo.renderPass = VulkanRenderer::getSwapchainRenderPass();
	if (m_Specification.Framebuffer)
		pipelineInfo.renderPass = m_Specification.Framebuffer->getRenderPass();

	if (vkCreateGraphicsPipelines(VulkanDevice::getVulkanDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		assert(false);
}

VulkanRenderPipeline::~VulkanRenderPipeline()
{
	VkDevice device = VulkanDevice::getVulkanDevice();
	vkDestroyPipeline(device, m_Pipeline, nullptr);
	vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
}
