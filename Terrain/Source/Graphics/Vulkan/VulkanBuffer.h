#pragma once
#include "VulkanBaseBuffer.h"

#include <vulkan/vulkan.h>
#include <memory>

class VulkanBuffer
{
public:
	VulkanBuffer(uint32_t size, BufferType type, BufferUsage usage);
	VulkanBuffer(void* data, uint32_t size, BufferType type, BufferUsage usage);
	~VulkanBuffer();

	void setData(void* data, uint32_t size);
	void setDataDirect(void* data, uint32_t size);

	void Map();
	void Unmap();

	VkBuffer getBuffer() const { return m_Buffer->m_Buffer; }
	std::shared_ptr<VulkanBaseBuffer> getBaseBuffer() const { return m_Buffer; }

private:
	void setDataGPUBuffer(void* data, uint32_t size);
	void setDataGPUCPUBuffer(void* data, uint32_t size);

	VkBufferUsageFlags getUsage();

private:
	std::shared_ptr<VulkanBaseBuffer> m_Buffer = nullptr;
	BufferType m_Type;
	BufferUsage m_Usage;

	void* m_MappedData = nullptr;
};