#include "VulkanBaseBuffer.h"

#include <cassert>

#include "VulkanDevice.h"
#include "VulkanUtils.h"

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memFlags)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(VulkanDevice::getVulkanContext()->getGPU(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & memFlags) == memFlags) {
			return i;
		}
	}

	throw (false);
}

VulkanBaseBuffer::VulkanBaseBuffer(const BufferProperties& props) : m_Properties(props)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_Properties.bufferSize;
	bufferInfo.usage = m_Properties.Usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkDevice vulkanDevice = VulkanDevice::getVulkanDevice();

	if (vkCreateBuffer(vulkanDevice, &bufferInfo, nullptr, &m_Buffer) != VK_SUCCESS)
		throw(false);

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(vulkanDevice, m_Buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, m_Properties.MemProperties);

	if (vkAllocateMemory(vulkanDevice, &allocInfo, nullptr, &m_DeviceMemory) != VK_SUCCESS)
		throw(false);

	vkBindBufferMemory(vulkanDevice, m_Buffer, m_DeviceMemory, 0);
}

VulkanBaseBuffer::~VulkanBaseBuffer()
{
	VkDevice vulkanDevice = VulkanDevice::getVulkanDevice();

	vkDestroyBuffer(vulkanDevice, m_Buffer, nullptr);
	vkFreeMemory(vulkanDevice, m_DeviceMemory, nullptr);
}

void VulkanBaseBuffer::Copy(const VulkanBaseBuffer& other)
{
	VkCommandBuffer buffer = VkUtils::beginSingleTimeCommand();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = other.m_Properties.bufferSize;
	vkCmdCopyBuffer(buffer, other.m_Buffer, m_Buffer, 1, &copyRegion);

	VkUtils::endSingleTimeCommand(buffer);
}

void VulkanBaseBuffer::Map(void*& data)
{
	vkMapMemory(VulkanDevice::getVulkanDevice(), m_DeviceMemory, 0, m_Properties.bufferSize, 0, &data);
}

void VulkanBaseBuffer::Unmap()
{
	vkUnmapMemory(VulkanDevice::getVulkanDevice(), m_DeviceMemory);
}
