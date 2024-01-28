#pragma once
#include "VulkanBaseBuffer.h"

#include <vulkan/vulkan.h>
#include <memory>

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
	std::shared_ptr<VulkanBaseBuffer> m_Buffer = nullptr;
	BufferType m_Type;

	void* m_Storage = nullptr;
};