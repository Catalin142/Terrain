#pragma once
#include "VulkanBaseBuffer.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class VulkanUniformBuffer
{
public:
	VulkanUniformBuffer(uint32_t size);
	~VulkanUniformBuffer();

	void setData(void* data, uint32_t size);

	VkBuffer getBuffer() { return m_Buffer->m_Buffer; }
	uint32_t getSize() { return m_Buffer->m_Properties.bufferSize; }

private:
	std::shared_ptr<VulkanBaseBuffer> m_Buffer;
	void* m_Storage;
};

class VulkanUniformBufferSet
{
public:
	VulkanUniformBufferSet(uint32_t size, uint32_t framesInFlight);
	~VulkanUniformBufferSet();

	void setData(void* data, uint32_t size, uint32_t frame);

	VkBuffer getBuffer(uint32_t frame) { return m_UniformBuffers[frame]->getBuffer(); }
	uint32_t getFrameCount() { return (uint32_t)m_UniformBuffers.size(); }
	uint32_t getBufferSize() { return m_UniformBuffers[0]->getSize(); }

private:
	std::vector<std::shared_ptr<VulkanUniformBuffer>> m_UniformBuffers;
};