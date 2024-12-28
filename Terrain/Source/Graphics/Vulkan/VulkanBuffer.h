#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

enum BufferMemoryUsage : uint32_t
{
	BUFFER_ONLY_GPU			= 1 << 0,
	BUFFER_CPU_VISIBLE		= 1 << 1,
	BUFFER_CPU_COHERENT		= 1 << 2,
	BUFFER_CPU_CACHED		= 1 << 3,
};

enum BufferType : uint32_t
{
	VERTEX_BUFFER			= 1 << 0,
	INDEX_BUFFER			= 1 << 1,
	INDIRECT_BUFFER			= 1 << 2,
	TRANSFER_SRC_BUFFER		= 1 << 3,
	TRANSFER_DST_BUFFER		= 1 << 4,
	STORAGE_BUFFER			= 1 << 5,
	UNIFORM_BUFFER			= 1 << 6,
};

struct VulkanBufferProperties
{
	uint32_t Size;
	uint32_t Type;
	uint32_t Usage;
};

class VulkanBuffer
{
public:
	VulkanBuffer(const VulkanBufferProperties& props);
	VulkanBuffer(void* data, const VulkanBufferProperties& props);
	~VulkanBuffer();

	// if the data is only on the gpu, we can do that only with staging buffer
	void setDataGPU(VkCommandBuffer cmdBuffer, void* data, uint32_t size);
	void setDataGPU(void* data, uint32_t size);

	void setDataCPU(VkCommandBuffer cmdBuffer, void* data, uint32_t size);
	void setDataCPU(void* data, uint32_t size);

	void setData(void* data, uint32_t size);
	void setDataDirect(void* data, uint32_t size);

	void Map();
	void Unmap();

	VkBuffer getBuffer() const { return m_Buffer; }
	void* getMappedData() { return m_MappedData; }
	uint32_t getSize() { return m_Properties.Size; }

private:
	VkBufferUsageFlags getUsage();
	VkMemoryPropertyFlags getMemoryUsage();

private:
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_DeviceMemory = VK_NULL_HANDLE;

	VulkanBufferProperties m_Properties;

	void* m_MappedData;
};

class VulkanBufferSet
{
public:
	VulkanBufferSet(uint32_t frameCount, const VulkanBufferProperties& props);

	uint32_t getCount();
	uint32_t getBufferSize();
	VkBuffer getBuffer(uint32_t index);

private:
	std::vector<std::shared_ptr<VulkanBuffer>> m_Buffers;
	uint32_t m_Size;
};