#include "VulkanBuffer.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"
#include "VulkanRenderer.h"

#include "Core/VulkanMemoryTracker.h"

#include <memory>
#include <cassert>

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memFlags)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(VulkanDevice::getVulkanContext()->getPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & memFlags) == memFlags) {
			return i;
		}
	}

	assert(false);
	return 0;
}

static void createBuffer(VkBufferCreateInfo bufferInfo, VkBuffer& buffer, VkDeviceMemory& memory, VkMemoryPropertyFlags memFlags)
{
	VkDevice vulkanDevice = VulkanDevice::getVulkanDevice();

	if (vkCreateBuffer(vulkanDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		assert(false);

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(vulkanDevice, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memFlags);

	if (vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS)
		assert(false);

	vkBindBufferMemory(vulkanDevice, buffer, memory, 0);
}

VkMemoryPropertyFlags VulkanBuffer::getMemoryUsage()
{
	VkMemoryPropertyFlags flags = 0x0;

	if (m_Properties.Usage & BufferMemoryUsage::BUFFER_ONLY_GPU)
		flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (m_Properties.Usage & BufferMemoryUsage::BUFFER_CPU_CACHED)
		flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

	if (m_Properties.Usage & BufferMemoryUsage::BUFFER_CPU_VISIBLE)
		flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if (m_Properties.Usage & BufferMemoryUsage::BUFFER_CPU_COHERENT)
		flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	return flags;
}

VkBufferUsageFlags VulkanBuffer::getUsage()
{
	VkBufferUsageFlags flags = 0x0;

	if (m_Properties.Type & BufferType::VERTEX_BUFFER)
		flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if (m_Properties.Type & BufferType::INDEX_BUFFER)
		flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if (m_Properties.Type & BufferType::INDIRECT_BUFFER)
		flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if (m_Properties.Type & BufferType::TRANSFER_DST_BUFFER)
		flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if (m_Properties.Type & BufferType::TRANSFER_SRC_BUFFER)
		flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (m_Properties.Type & BufferType::STORAGE_BUFFER)
		flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if (m_Properties.Type & BufferType::UNIFORM_BUFFER)
		flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	return flags;
}

VulkanBuffer::VulkanBuffer(const VulkanBufferProperties& props) : m_Properties(props)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_Properties.Size;
	bufferInfo.usage = getUsage();
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	TRACK_VK_MEMORY(m_Properties.Size);

	createBuffer(bufferInfo, m_Buffer, m_DeviceMemory, getMemoryUsage());
}

VulkanBuffer::VulkanBuffer(void* data, const VulkanBufferProperties& props) : VulkanBuffer(props)
{
	TRACK_VK_MEMORY(m_Properties.Size);

	setData(data, m_Properties.Size);
}

VulkanBuffer::~VulkanBuffer()
{
	VkDevice vulkanDevice = VulkanDevice::getVulkanDevice();

	vkDestroyBuffer(vulkanDevice, m_Buffer, nullptr);
	vkFreeMemory(vulkanDevice, m_DeviceMemory, nullptr);
}

void VulkanBuffer::setDataGPU(void* data, uint32_t size)
{
	VkCommandBuffer cmdBuffer = VulkanUtils::beginSingleTimeCommand();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo stagingInfo{};
	stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingInfo.size = size;
	stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	createBuffer(stagingInfo, stagingBuffer, stagingMemory, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* stagingBufferData;

	vkMapMemory(VulkanDevice::getVulkanDevice(), stagingMemory, 0, size, 0, &stagingBufferData);
	memcpy(stagingBufferData, data, (size_t)size);
	vkUnmapMemory(VulkanDevice::getVulkanDevice(), stagingMemory);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(cmdBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);

	VulkanUtils::endSingleTimeCommand(cmdBuffer);

	vkDestroyBuffer(VulkanDevice::getVulkanDevice(), stagingBuffer, nullptr);
	vkFreeMemory(VulkanDevice::getVulkanDevice(), stagingMemory, nullptr);
}

void VulkanBuffer::setDataCPU(VkCommandBuffer cmdBuffer, void* data, uint32_t size)
{
	vkCmdUpdateBuffer(cmdBuffer, m_Buffer, 0, size, data);
}

void VulkanBuffer::setDataCPU(void* data, uint32_t size)
{
	void* bufferData;
	vkMapMemory(VulkanDevice::getVulkanDevice(), m_DeviceMemory, 0, m_Properties.Size, 0, &bufferData);
	memcpy(bufferData, data, (size_t)size);
	vkUnmapMemory(VulkanDevice::getVulkanDevice(), m_DeviceMemory);
}

void VulkanBuffer::setData(void* data, uint32_t size)
{
	if (m_Properties.Usage & BufferMemoryUsage::BUFFER_ONLY_GPU)
		setDataGPU(data, size);
	else
		setDataCPU(data, size);
}

void VulkanBuffer::setDataDirect(void* data, uint32_t size)
{
	memcpy(m_MappedData, data, (size_t)size);
}

void VulkanBuffer::Map()
{
	vkMapMemory(VulkanDevice::getVulkanDevice(), m_DeviceMemory, 0, m_Properties.Size, 0, &m_MappedData);
}

void VulkanBuffer::Unmap()
{
	vkUnmapMemory(VulkanDevice::getVulkanDevice(), m_DeviceMemory);
	m_MappedData = nullptr;
}

VulkanBufferSet::VulkanBufferSet(uint32_t frameCount, const VulkanBufferProperties& props) : m_Size(props.Size)
{
	for (uint32_t index = 0; index < frameCount; index++)
		m_Buffers.push_back(std::make_shared<VulkanBuffer>(props));
}

uint32_t VulkanBufferSet::getCount()
{
	return (uint32_t)m_Buffers.size();
}

uint32_t VulkanBufferSet::getBufferSize()
{
	return m_Size;
}

VkBuffer VulkanBufferSet::getVkBuffer(uint32_t index)
{
	assert(index < (uint32_t)m_Buffers.size());
	return m_Buffers[index]->getBuffer();
}

const std::shared_ptr<VulkanBuffer>& VulkanBufferSet::getBuffer(uint32_t index)
{
	assert(index < (uint32_t)m_Buffers.size());
	return m_Buffers[index];
}

const std::shared_ptr<VulkanBuffer>& VulkanBufferSet::getCurrentFrameBuffer()
{
	uint32_t index = VulkanRenderer::getCurrentFrame();
	index %= (uint32_t)m_Buffers.size();
	return m_Buffers[index];
}
