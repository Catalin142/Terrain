#include "VulkanBuffer.h"
#include <memory>
#include <cassert>

VulkanBuffer::VulkanBuffer(void* data, uint32_t size, BufferType type, BufferUsage usage) : m_Type(type)
{
	switch (usage)
	{
	case BufferUsage::STATIC:
		createGPUBuffer(data, size);
		break;

	case BufferUsage::DYNAMIC:
		createGPUCPUBuffer(data, size);
		break;

	default:
		assert(false);
	}
}

VulkanBuffer::~VulkanBuffer()
{
	if (m_Storage)
		delete[] m_Storage;
}

void VulkanBuffer::setData(void* data, uint32_t size)
{
	std::memcpy(m_Storage, data, size);

	void* mappedData = nullptr;
	m_Buffer->Map(mappedData);
	std::memcpy(mappedData, m_Storage, size);
	m_Buffer->Unmap();
}

void VulkanBuffer::createGPUBuffer(void* data, uint32_t size)
{
	BufferProperties stagingBufferProps;
	stagingBufferProps.bufferSize = size;
	stagingBufferProps.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferProps.MemProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VulkanBaseBuffer stagingBuffer(stagingBufferProps);

	void* stagingBufferData;

	stagingBuffer.Map(stagingBufferData);
	memcpy(stagingBufferData, data, (size_t)size);
	stagingBuffer.Unmap();

	BufferProperties vertexBufferProps;
	vertexBufferProps.bufferSize = size;
	vertexBufferProps.Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | getUsage();
	vertexBufferProps.MemProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	m_Buffer = std::make_shared<VulkanBaseBuffer>(vertexBufferProps);
	m_Buffer->Copy(stagingBuffer);

}

void VulkanBuffer::createGPUCPUBuffer(void* data, uint32_t size)
{
	BufferProperties vertexBufferProps;
	vertexBufferProps.bufferSize = size;
	vertexBufferProps.Usage = getUsage();
	vertexBufferProps.MemProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	m_Buffer = std::make_shared<VulkanBaseBuffer>(vertexBufferProps);
	
	m_Storage = new uint8_t[size];

	setData(data, size);
}

VkBufferUsageFlags VulkanBuffer::getUsage()
{
	switch (m_Type)
	{
	case BufferType::VERTEX: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case BufferType::INDEX: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case BufferType::INDIRECT: return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	default: return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;

	}
}
