#include "VulkanVertexBufferLayout.h"

#include <cassert>

VulkanVertexBufferLayout::VulkanVertexBufferLayout(std::initializer_list<VertexType> input)
{
	uint32_t stride = 0;
	for (VertexType type : input)
		stride += getTypeSize(type);

	m_BindingDescription.binding = 0;
	m_BindingDescription.stride = stride;
	m_BindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


	m_InputAttributeDescriptions.resize(input.size());

	uint32_t offset = 0;
	uint32_t location = 0;
	for (VertexType type : input)
	{
		m_InputAttributeDescriptions[location].binding = 0;
		m_InputAttributeDescriptions[location].location = location;
		m_InputAttributeDescriptions[location].format = getVulkanFormat(type);
		m_InputAttributeDescriptions[location].offset = offset;

		offset += getTypeSize(type);
		location++;
	}
}

uint32_t getTypeSize(VertexType type)
{
	switch (type)
	{
	case VertexType::INT: return sizeof(int32_t);
	case VertexType::INT_2: return 2 * sizeof(int32_t);
	case VertexType::INT_3: return 3 * sizeof(int32_t);
	case VertexType::INT_4: return 4 * sizeof(int32_t);

	case VertexType::FLOAT: return sizeof(float);
	case VertexType::FLOAT_2: return 2 * sizeof(float);
	case VertexType::FLOAT_3: return 3 * sizeof(float);
	case VertexType::FLOAT_4: return 4 * sizeof(float);
	
	default: assert(false);
	}

	return 0;
}

VkFormat getVulkanFormat(VertexType type)
{
	switch (type)
	{
	case VertexType::INT: return VK_FORMAT_R32_SINT;
	case VertexType::INT_2: return VK_FORMAT_R32G32_SINT;
	case VertexType::INT_3: return VK_FORMAT_R32G32B32_SINT;
	case VertexType::INT_4: return VK_FORMAT_R32G32B32A32_SINT;

	case VertexType::FLOAT: return VK_FORMAT_R32_SFLOAT;
	case VertexType::FLOAT_2: return VK_FORMAT_R32G32_SFLOAT;
	case VertexType::FLOAT_3: return VK_FORMAT_R32G32B32_SFLOAT;
	case VertexType::FLOAT_4: return VK_FORMAT_R32G32B32A32_SFLOAT;

	default: assert(false);
	}

	return VK_FORMAT_UNDEFINED;
}
