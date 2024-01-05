#include "VulkanUniformBuffer.h"
#include <memory>
#include <cassert>

VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size)
{
	BufferProperties props;
	props.bufferSize = size;
	props.MemProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	props.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	m_Buffer = new VulkanBaseBuffer(props);

	m_Storage = new uint8_t[size];
}

VulkanUniformBuffer::~VulkanUniformBuffer()
{ 
	delete m_Buffer;
	delete[] m_Storage;
}

void VulkanUniformBuffer::setData(void* data, uint32_t size)
{
	std::memcpy(m_Storage, data, size);

	void* mappedData = nullptr;
	m_Buffer->Map(mappedData);
	std::memcpy(mappedData, m_Storage, size);
	m_Buffer->Unmap();
}

VulkanUniformBufferSet::VulkanUniformBufferSet(uint32_t size, uint32_t framesInFlight)
{
	m_UniformBuffers.resize(framesInFlight);
	for (uint32_t frame = 0; frame < framesInFlight; frame++)
		m_UniformBuffers[frame] = std::make_shared<VulkanUniformBuffer>(size);
}

VulkanUniformBufferSet::~VulkanUniformBufferSet()
{
	m_UniformBuffers.clear();
}

void VulkanUniformBufferSet::setData(void* data, uint32_t size, uint32_t frame)
{
	assert(frame < m_UniformBuffers.size());
	m_UniformBuffers[frame]->setData(data, size);
}
