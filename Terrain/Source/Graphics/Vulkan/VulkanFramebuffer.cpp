#include "VulkanFramebuffer.h"

#include "VulkanDevice.h"

static bool hasDepth(VkFormat format)
{
	static const std::vector<VkFormat> formats =
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};
	return std::find(formats.begin(), formats.end(), format) != std::end(formats);
}

static bool hasStencil(VkFormat format)
{
	static const std::vector<VkFormat> formats =
	{
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};
	return std::find(formats.begin(), formats.end(), format) != std::end(formats);
}

VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& spec) : m_Specification(spec)
{
	///////////////////////////////////////////////////////////////////////
	// Render pass
	///////////////////////////////////////////////////////////////////////

	std::vector<VkAttachmentDescription> colorAttachments;
	VkAttachmentDescription depthAttachment{};

	bool hasDepth = m_Specification.DepthAttachment.Format != VK_FORMAT_UNDEFINED;

	// Color
	for (const FramebufferAttachment& colorAttachment : m_Specification.Attachments)
	{
		VkAttachmentDescription colorAttachmentInfo{};
		colorAttachmentInfo.loadOp = m_Specification.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachmentInfo.storeOp = colorAttachment.Sample ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentInfo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentInfo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentInfo.initialLayout = colorAttachment.IntialLayout;
		colorAttachmentInfo.format = colorAttachment.Format;
		colorAttachmentInfo.samples = VkSampleCountFlagBits(m_Specification.Samples);
		colorAttachmentInfo.finalLayout = colorAttachment.FinalLayout;

		colorAttachments.emplace_back(colorAttachmentInfo);
	}

	// Depth stencil
	{
		if (hasDepth)
		{
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = m_Specification.DepthAttachment.IntialLayout;
			depthAttachment.format = m_Specification.DepthAttachment.Format;
			depthAttachment.samples = VkSampleCountFlagBits(m_Specification.Samples);
			depthAttachment.finalLayout = m_Specification.DepthAttachment.FinalLayout;
		}
	}

	std::vector<VkAttachmentReference> colorAttachmentRefs(colorAttachments.size());
	VkAttachmentReference depthAttachmentRef{};

	// Color ref
	uint32_t attachmentIndex = 0;
	for (attachmentIndex = 0; attachmentIndex < colorAttachments.size(); attachmentIndex++)
	{
		colorAttachmentRefs[attachmentIndex] = VkAttachmentReference{};
		colorAttachmentRefs[attachmentIndex].attachment = attachmentIndex;
		colorAttachmentRefs[attachmentIndex].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	// Depth stencil ref
	{
		depthAttachmentRef.attachment = attachmentIndex;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	// Subpass
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = (uint32_t)colorAttachments.size();
	subpass.pColorAttachments = colorAttachmentRefs.data();
	if (hasDepth)
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

	std::vector<VkAttachmentDescription> attachmentDescriptions = colorAttachments;
	if (hasDepth)
		attachmentDescriptions.push_back(depthAttachment);

	// for offscreen rendering
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments = attachmentDescriptions.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = uint32_t(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	vkCreateRenderPass(VulkanDevice::getVulkanDevice(), &renderPassInfo, nullptr, &m_RenderPass);

	/////////////////////////////////////////////////////////////////////////////
	// Create framebuffer
	/////////////////////////////////////////////////////////////////////////////
	Create();
}

VulkanFramebuffer::~VulkanFramebuffer()
{
	vkDestroyFramebuffer(VulkanDevice::getVulkanDevice(), m_Framebuffer, nullptr);
	vkDestroyRenderPass(VulkanDevice::getVulkanDevice(), m_RenderPass, nullptr);
}

void VulkanFramebuffer::Create()
{
	m_ColorAttachments.clear();

	std::vector<VkImageView> attachments;
	bool hasDepth = false;

	///////////////////////////////////////////////////////////////////////
	// Framebuffer
	///////////////////////////////////////////////////////////////////////

	for (const FramebufferAttachment& colorAttachment : m_Specification.Attachments)
	{
		m_Specification.hasAttachments = true;
		VulkanImageSpecification imgSpec{};
		imgSpec.Samples = m_Specification.Samples;
		imgSpec.Format = colorAttachment.Format;
		imgSpec.Width = m_Specification.Width;
		imgSpec.Height = m_Specification.Height;
		imgSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imgSpec.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		imgSpec.CreateSampler = colorAttachment.Sample;
		if (colorAttachment.Sample)
			imgSpec.UsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		m_ColorAttachments.emplace_back(std::make_shared<VulkanImage>(imgSpec));
		m_ColorAttachments.back()->Create();
		attachments.emplace_back(m_ColorAttachments.back()->getVkImageView());
	}

	if (m_Specification.DepthAttachment.Format != VK_FORMAT_UNDEFINED)
	{
		hasDepth = true;
		VulkanImageSpecification depthSpec{};
		depthSpec.Samples = m_Specification.Samples;
		depthSpec.Format = m_Specification.DepthAttachment.Format;
		depthSpec.Width = m_Specification.Width;
		depthSpec.Height = m_Specification.Height;

		depthSpec.Aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (hasStencil(m_Specification.DepthAttachment.Format))
			depthSpec.Aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

		depthSpec.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (m_Specification.DepthAttachment.Sample)
			depthSpec.UsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		depthSpec.CreateSampler = m_Specification.DepthAttachment.Sample;
		if (m_Specification.DepthAttachment.Sample)
			depthSpec.UsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		m_DepthAttachment = std::make_shared<VulkanImage>(depthSpec);
		m_DepthAttachment->Create();
		attachments.emplace_back(m_DepthAttachment->getVkImageView());
	}

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.attachmentCount = (uint32_t)attachments.size();
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = m_Specification.Width;
	framebufferInfo.height = m_Specification.Height;
	framebufferInfo.layers = 1;
	framebufferInfo.renderPass = m_RenderPass;
	vkCreateFramebuffer(VulkanDevice::getVulkanDevice(), &framebufferInfo, nullptr, &m_Framebuffer);
}

void VulkanFramebuffer::Release()
{
	if (m_Specification.hasAttachments)
		m_ColorAttachments.clear();

	if (m_DepthAttachment)
		m_DepthAttachment.reset();
	
	vkDestroyFramebuffer(VulkanDevice::getVulkanDevice(), m_Framebuffer, nullptr);
}

void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
{
	Release();

	m_Specification.Width = width;
	m_Specification.Height = height;

	Create();
}
