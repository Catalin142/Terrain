#include "VulkanRenderPass.h"

void VulkanRenderPass::Resize(uint32_t width, uint32_t height)
{
	m_Pipeline->getTargetFramebuffer()->Resize(width, height);
}

std::shared_ptr<VulkanFramebuffer> VulkanRenderPass::getTargetFramebuffer()
{
	return m_Pipeline->getTargetFramebuffer();
}

std::shared_ptr<VulkanImage> VulkanRenderPass::getOutput(uint32_t index)
{
	return m_Pipeline->getTargetFramebuffer()->getImage(index);
}

std::shared_ptr<VulkanImage> VulkanRenderPass::getDepthOutput()
{
	return m_Pipeline->getTargetFramebuffer()->getDepthImage();
}
