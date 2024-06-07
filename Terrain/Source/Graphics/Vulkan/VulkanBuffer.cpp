#include "VulkanBuffer.h"
#include <memory>
#include <cassert>

VkMemoryPropertyFlags getFlag(BufferUsage usage)
{
	switch (usage)
	{
	case BufferUsage::STATIC: return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	case BufferUsage::DYNAMIC: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}
}

VulkanBuffer::VulkanBuffer(uint32_t size, BufferType type, BufferUsage usage) : m_Type(type), m_Usage(usage)
{
	BufferProperties vertexBufferProps;
	vertexBufferProps.bufferSize = size;
	vertexBufferProps.Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | getUsage();
	vertexBufferProps.MemProperties = getFlag(usage);

	m_Buffer = std::make_shared<VulkanBaseBuffer>(vertexBufferProps);
}

VulkanBuffer::VulkanBuffer(void* data, uint32_t size, BufferType type, BufferUsage usage) : m_Type(type), m_Usage(usage)
{
	BufferProperties vertexBufferProps;
	vertexBufferProps.bufferSize = size;
	vertexBufferProps.Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | getUsage();
	vertexBufferProps.MemProperties = getFlag(usage);

	m_Buffer = std::make_shared<VulkanBaseBuffer>(vertexBufferProps);

	setData(data, size);
}

VulkanBuffer::~VulkanBuffer()
{
}

void VulkanBuffer::setData(void* data, uint32_t size)
{
	switch (m_Usage)
	{
	case BufferUsage::STATIC:
		setDataGPUBuffer(data, size);
		return;

	case BufferUsage::DYNAMIC:
		setDataGPUCPUBuffer(data, size);
		return;

	default:
		assert(false);
	}
}

void VulkanBuffer::setDataDirect(void* data, uint32_t size)
{
	memcpy(m_MappedData, data, (size_t)size);
}

void VulkanBuffer::Map()
{
	m_Buffer->Map(m_MappedData);
}

void VulkanBuffer::Unmap()
{
	m_Buffer->Unmap();
	m_MappedData = nullptr;
}

void VulkanBuffer::setDataGPUBuffer(void* data, uint32_t size)
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

	m_Buffer->Copy(stagingBuffer);
}

void VulkanBuffer::setDataGPUCPUBuffer(void* data, uint32_t size)
{
	void* bufferData;
	m_Buffer->Map(bufferData);
	memcpy(bufferData, data, (size_t)size);
	m_Buffer->Unmap();
}

VkBufferUsageFlags VulkanBuffer::getUsage()
{
	switch (m_Type)
	{
	case BufferType::VERTEX: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case BufferType::INDEX: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case BufferType::INDIRECT: return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	case BufferType::TRANSFER_SRC: return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	default: return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;

	}
}
