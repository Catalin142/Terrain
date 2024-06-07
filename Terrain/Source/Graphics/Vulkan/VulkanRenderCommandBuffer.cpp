#include "VulkanRenderCommandBuffer.h"

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderer.h"

#define MAX_NUMBER_OF_QUERIES 16

VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(uint32_t count)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = VulkanDevice::getVulkanContext()->getGraphicsFamilyIndex();

	if (vkCreateCommandPool(VulkanDevice::getVulkanDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
		assert(false);

	m_CommandBuffers.resize(count);
	VkCommandBufferAllocateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferInfo.commandPool = VulkanDevice::getGraphicsCommandPool();
	bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferInfo.commandBufferCount = count; 

	m_CommandBuffers.resize(count);
	if (vkAllocateCommandBuffers(VulkanDevice::getVulkanDevice(), &bufferInfo, m_CommandBuffers.data()) != VK_SUCCESS)
		assert(false);

	// TODO: Get actual frames in flight from a static renderer confing
	uint32_t framesInFlight = VulkanRenderer::getFramesInFlight();
	m_inFlightFences.resize(framesInFlight);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // l face in signaled state

	for (uint32_t i = 0; i < framesInFlight; i++)
		if (vkCreateFence(VulkanDevice::getVulkanDevice(), &fenceInfo, nullptr, &m_inFlightFences[i])) assert(false);
}

VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(bool swapchain) : m_OwnedBySwapchain(swapchain)
{
	m_QueryResults.resize(MAX_NUMBER_OF_QUERIES, 0.0f);

	m_TimeStamps.resize(VulkanRenderer::getFramesInFlight());
	for (auto& ts : m_TimeStamps)
		ts.resize(MAX_NUMBER_OF_QUERIES * 2);

	VkQueryPoolCreateInfo query_pool_info{};
	query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	query_pool_info.queryCount = 2 * MAX_NUMBER_OF_QUERIES;

	m_QueryPools.resize(2);
	for (VkQueryPool& queryPool : m_QueryPools)
		vkCreateQueryPool(VulkanDevice::getVulkanDevice(), &query_pool_info, nullptr, &queryPool);
}

VulkanRenderCommandBuffer::~VulkanRenderCommandBuffer()
{
	for (VkQueryPool& queryPool : m_QueryPools)
		vkDestroyQueryPool(VulkanDevice::getVulkanDevice(), queryPool, nullptr);

	if (m_OwnedBySwapchain)
		return;

	vkDestroyCommandPool(VulkanDevice::getVulkanDevice(), m_CommandPool, nullptr);
	for (size_t i = 0; i < m_inFlightFences.size(); i++)
		vkDestroyFence(VulkanDevice::getVulkanDevice(), m_inFlightFences[i], nullptr);
}

void VulkanRenderCommandBuffer::Begin()
{
	uint32_t currentFrameIndex = VulkanRenderer::getCurrentFrame();

	VkCommandBuffer commandBuffer;
	if (m_OwnedBySwapchain) commandBuffer = VulkanRenderer::getSwapchainCurrentCommandBuffer();
	else commandBuffer = m_CommandBuffers[currentFrameIndex];

	m_CurrentCommandBuffer = commandBuffer;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		assert(false);

	vkCmdResetQueryPool(commandBuffer, m_QueryPools[currentFrameIndex], 0, MAX_NUMBER_OF_QUERIES);
	m_CurrentAvailableQuery = 2;

	// Render time of the command buffer
	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[currentFrameIndex], 0);
	m_QueryFrame = currentFrameIndex;
}

void VulkanRenderCommandBuffer::End()
{
	VkCommandBuffer commandBuffer;
	if (m_OwnedBySwapchain) commandBuffer = VulkanRenderer::getSwapchainCurrentCommandBuffer();
	else commandBuffer = m_CommandBuffers[m_QueryFrame];

	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[m_QueryFrame], 1);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		assert(false);
}

static std::mutex queueMutex;
void VulkanRenderCommandBuffer::Submit()
{
	if (m_OwnedBySwapchain)
		return;

	VkDevice device = VulkanDevice::getVulkanDevice();

	vkWaitForFences(device, 1, &m_inFlightFences[m_QueryFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &m_inFlightFences[m_QueryFrame]);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = m_CommandBuffers[m_QueryFrame];
	submitInfo.pCommandBuffers = &commandBuffer;


	{
		std::lock_guard<std::mutex> lock(queueMutex);
		if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getGraphicsQueue(), 1, &submitInfo,
			m_inFlightFences[m_QueryFrame]) != VK_SUCCESS)
			assert(false);
	}
}

void VulkanRenderCommandBuffer::beginQuery(const std::string& name)
{
	vkCmdWriteTimestamp(m_CurrentCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[m_QueryFrame], m_CurrentAvailableQuery);
	m_QueryStart[name] = m_CurrentAvailableQuery;
	m_CurrentAvailableQuery += 2;
}

void VulkanRenderCommandBuffer::endQuery(const std::string& name)
{
	if (m_QueryStart.find(name) == m_QueryStart.end())
		return;

	uint32_t startIndex = m_QueryStart[name];
	vkCmdWriteTimestamp(m_CurrentCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[m_QueryFrame], startIndex + 1);
}

void VulkanRenderCommandBuffer::queryResults()
{
	VkResult res = vkGetQueryPoolResults(VulkanDevice::getVulkanDevice(), m_QueryPools[m_QueryFrame], 0, MAX_NUMBER_OF_QUERIES,
		MAX_NUMBER_OF_QUERIES * 2 * sizeof(uint64_t), m_TimeStamps[m_QueryFrame].data(), sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT);

	for (uint32_t i = 0; i < m_CurrentAvailableQuery; i += 2)
	{
		uint64_t startTime = m_TimeStamps[m_QueryFrame][i];
		uint64_t endTime = m_TimeStamps[m_QueryFrame][i + 1];
		float nsTime = endTime > startTime ? (endTime - startTime) * VulkanDevice::getVulkanContext()->getPhysicalDeviceLimits().timestampPeriod : 0.0f;
		nsTime /= 1000000.0f;
		m_QueryResults[i / 2] = nsTime;
	}
}

float VulkanRenderCommandBuffer::getCommandBufferTime()
{
	return m_QueryResults[0];
}

float VulkanRenderCommandBuffer::getTime(const std::string& name)
{
	if (m_QueryStart.find(name) == m_QueryStart.end())
		return 0.0f;
	uint32_t startIndex = m_QueryStart[name];
	return m_QueryResults[startIndex / 2];
}
