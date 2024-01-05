#pragma once
#include <vulkan/vulkan.h>
#include "VulkanBaseBuffer.h"

class VulkanBuffer
{
public:
	VulkanBuffer(void* data, uint32_t size, BufferType type, BufferUsage usage);
	~VulkanBuffer();

	void setData(void* data, uint32_t size);

	VkBuffer getBuffer() const { return m_Buffer->m_Buffer; }

private:
	void createGPUBuffer(void* data, uint32_t size);
	void createGPUCPUBuffer(void* data, uint32_t size);

	VkBufferUsageFlags getUsage();

private:
	VulkanBaseBuffer* m_Buffer = nullptr;
	BufferType m_Type;

	void* m_Storage = nullptr;
};