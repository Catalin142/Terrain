#pragma once
#include "VulkanImage.h"

#include <memory>
#include <vector>
#include <glm/glm.hpp>

enum class BlendMode
{
	NONE,
	SRC_ALPHA_ONE_MINUS_SRC_ALPHA,
	ONE_ZERO,
	ZERO_SRC_COLOR,
};

struct FramebufferAttachment
{
	VkFormat Format = VK_FORMAT_UNDEFINED;
	bool Blend = true;
	bool Sample = false;
	BlendMode blendMode = BlendMode::SRC_ALPHA_ONE_MINUS_SRC_ALPHA;
};

struct FramebufferSpecification
{
	uint32_t Width;
	uint32_t Height;

	uint32_t Samples;

	glm::vec4 colorClear = { 0.0f, 0.0f, 0.0f, 1.0f };
	float depthClear = 1.0f;

	std::vector<FramebufferAttachment> Attachments;
	bool hasAttachments = false;
	FramebufferAttachment DepthAttachment;
};

class VulkanFramebuffer
{
public:
	VulkanFramebuffer(const FramebufferSpecification& spec);
	~VulkanFramebuffer();

	void Create();
	void Release();
	void Resize(uint32_t width, uint32_t height);

	std::shared_ptr<VulkanImage> getImage(uint32_t index) { return m_ColorAttachments[index]; }
	std::shared_ptr<VulkanImage> getDepthImage() { return m_DepthAttachment; }

	VkFramebuffer getVkFramebuffer() { return m_Framebuffer; }
	VkRenderPass getRenderPass() { return m_RenderPass; }

	const FramebufferSpecification& getSpecification() const { return m_Specification; }

private:
	FramebufferSpecification m_Specification;

	VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;

	std::vector<std::shared_ptr<VulkanImage>> m_ColorAttachments;
	std::shared_ptr<VulkanImage> m_DepthAttachment;
};