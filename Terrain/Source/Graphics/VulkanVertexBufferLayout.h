#pragma once
#include <initializer_list>
#include <vector>
#include <vulkan/vulkan.h>

enum class VertexType
{
	INT,
	INT_2,
	INT_3, 
	INT_4,

	FLOAT,
	FLOAT_2,
	FLOAT_3,
	FLOAT_4,
};

static uint32_t getTypeSize(VertexType type);
static VkFormat getVulkanFormat(VertexType type);

class VulkanVertexBufferLayout
{
public:
	VulkanVertexBufferLayout() = default;
	VulkanVertexBufferLayout(std::initializer_list<VertexType> input);
	~VulkanVertexBufferLayout() = default;

	VkVertexInputBindingDescription getBindindDescription() const { return m_BindingDescription; }
	const std::vector<VkVertexInputAttributeDescription>& getInputAttributeDescriptions() const { return m_InputAttributeDescriptions; }

private:
	VkVertexInputBindingDescription m_BindingDescription;
	std::vector<VkVertexInputAttributeDescription> m_InputAttributeDescriptions;
};
