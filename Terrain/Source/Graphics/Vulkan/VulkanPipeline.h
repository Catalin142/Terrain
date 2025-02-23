#pragma once

#include "VulkanShader.h"
#include "VulkanVertexBufferLayout.h"
#include "VulkanFramebuffer.h"

#include <memory>

enum class PrimitiveTopology
{
	POINTS,
	LINES,
	TRIANGLES,
	PATCHES,
};

static VkPrimitiveTopology getVulkanTopology(PrimitiveTopology topology);

enum class DepthCompare
{
	NEVER,
	NOT_EQUAL,
	LESS,
	GRATER,
	EQUAL,
	ALWAYS,
};

static VkCompareOp getVulkanDepthCompareOperation(DepthCompare comp);

struct PushConstant
{
	uint32_t Size;
	VkShaderStageFlagBits Stage;
};

struct PipelineSpecification
{
	std::shared_ptr<VulkanShader> Shader;
	std::shared_ptr<VulkanFramebuffer> Framebuffer;
	VulkanVertexBufferLayout vertexBufferLayout;
	PrimitiveTopology Topology = PrimitiveTopology::TRIANGLES;
	DepthCompare depthCompareFunction = DepthCompare::NEVER;

	std::vector<PushConstant> pushConstants;

	bool depthTest = true;
	bool depthWrite = true;
	bool Culling = false;
	bool Wireframe = false;
	float lineWidth = 1.0f;
	uint32_t tessellationControlPoints = 0;
};

class VulkanPipeline
{
public:
	VulkanPipeline(const PipelineSpecification& spec);
	~VulkanPipeline();

	const PipelineSpecification& getSpecification() { return m_Specification; }

	void Recreate();

	VkPipeline getVkPipeline() { return m_Pipeline; }
	VkPipelineLayout getVkPipelineLayout() { return m_PipelineLayout; }

	std::shared_ptr<VulkanFramebuffer> getTargetFramebuffer() const { return m_Specification.Framebuffer; }
	std::shared_ptr<VulkanShader> getShader() const { return m_Specification.Shader; }

private:
	PipelineSpecification m_Specification;

	VkPipeline m_Pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};
