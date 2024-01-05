#pragma once
#include <vulkan/vulkan.h>

enum class BufferUsage
{
	STATIC,
	DYNAMIC,
};

enum class BufferType
{
	VERTEX,
	INDEX,
};

struct BufferProperties
{
	uint32_t bufferSize;
	VkBufferUsageFlags Usage;
	VkMemoryPropertyFlags MemProperties;
};

class VulkanBaseBuffer
{
	friend class VulkanUniformBuffer;
	friend class VulkanBuffer;

public:
	VulkanBaseBuffer() = default;
	VulkanBaseBuffer(const BufferProperties& props);
	~VulkanBaseBuffer();

	void Copy(const VulkanBaseBuffer& other);

	void Map(void*& data);
	void Unmap();

	VkBuffer getBuffer() const { return m_Buffer; }

private:
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_DeviceMemory = VK_NULL_HANDLE;

	BufferProperties m_Properties;
};