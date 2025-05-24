#pragma once

#include "VulkanShader.h"
#include "VulkanVertexBufferLayout.h"
#include "VulkanFramebuffer.h"
#include "VulkanShaderInput.h"
#include "VulkanInitializers.h"

#include <memory>

struct PushConstantSpecification
{
	uint32_t Size;
	ShaderStage Stage;
};

struct RenderPipelineSpecification
{
	friend class VulkanRenderPipeline;

	std::shared_ptr<VulkanShader> Shader;
	std::shared_ptr<VulkanFramebuffer> Framebuffer;
	std::vector<PushConstantSpecification> pushConstants;
	
	VulkanVertexBufferLayout vertexBufferLayout = VulkanVertexBufferLayout{};
	PrimitiveTopology Topology = PrimitiveTopology::TRIANGLES;
	ResterizationStateSpecification resterizationSpecification = ResterizationStateSpecification{};
	uint32_t MMSASampleCount = 1;
	DepthStencilStateSpecification depthStateSpecification = DepthStencilStateSpecification{}; 
	uint32_t tessellationPatchControlPoints = 0;
};

class VulkanRenderPipeline
{
public:
	VulkanRenderPipeline(const RenderPipelineSpecification& spec);
	~VulkanRenderPipeline();

	const RenderPipelineSpecification& getSpecification() { return m_Specification; }

	VkPipeline getVkPipeline() { return m_Pipeline; }
	VkPipelineLayout getVkPipelineLayout() { return m_PipelineLayout; }

	std::shared_ptr<VulkanFramebuffer> getTargetFramebuffer() const { return m_Specification.Framebuffer; }
	std::shared_ptr<VulkanShader> getShader() const { return m_Specification.Shader; }

private:
	RenderPipelineSpecification m_Specification;

	VkPipeline m_Pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};
