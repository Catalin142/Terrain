#include "VulkanRenderCommandBuffer.h"

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

#include <cassert>

#define MAX_NUMBER_OF_QUERIES 128

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
	bufferInfo.commandPool = m_CommandPool;
	bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferInfo.commandBufferCount = count; 

	m_CommandBuffers.resize(count);
	if (vkAllocateCommandBuffers(VulkanDevice::getVulkanDevice(), &bufferInfo, m_CommandBuffers.data()) != VK_SUCCESS)
		assert(false);

	uint32_t framesInFlight = VulkanSwapchain::framesInFlight;
	m_inFlightFences.resize(framesInFlight);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < framesInFlight; i++)
		if (vkCreateFence(VulkanDevice::getVulkanDevice(), &fenceInfo, nullptr, &m_inFlightFences[i])) assert(false);

	m_QueryResults.resize(MAX_NUMBER_OF_QUERIES, 0.0f);

	m_TimeStamps.resize(VulkanSwapchain::framesInFlight);
	for (auto& ts : m_TimeStamps)
		ts.resize(MAX_NUMBER_OF_QUERIES * 2);

	VkQueryPoolCreateInfo query_pool_info{};
	query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	query_pool_info.queryCount = 2 * MAX_NUMBER_OF_QUERIES;

		vkCreateQueryPool(VulkanDevice::getVulkanDevice(), &query_pool_info, nullptr, &m_QueryPools);
}

VulkanRenderCommandBuffer::~VulkanRenderCommandBuffer()
{
	VkDevice vkDevice = VulkanDevice::getVulkanDevice();

	vkDeviceWaitIdle(vkDevice);

		vkDestroyQueryPool(vkDevice, m_QueryPools, nullptr);

	for (auto& [name, pool] : m_QueryPoolsPipelineStats)
		vkDestroyQueryPool(vkDevice, pool, nullptr);

	if (!m_CommandBuffers.empty()) {
		vkFreeCommandBuffers(vkDevice, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());
	}

	vkDestroyCommandPool(vkDevice, m_CommandPool, nullptr);

	for (size_t i = 0; i < m_inFlightFences.size(); i++)
		vkDestroyFence(vkDevice, m_inFlightFences[i], nullptr);

	vkDeviceWaitIdle(vkDevice);
}

void VulkanRenderCommandBuffer::Begin(uint32_t frameIndex)
{
	uint32_t currentFrameIndex = frameIndex;

	VkCommandBuffer commandBuffer;
	commandBuffer = m_CommandBuffers[currentFrameIndex];

	m_CurrentCommandBuffer = commandBuffer;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		assert(false);

	vkCmdResetQueryPool(commandBuffer, m_QueryPools, 0, MAX_NUMBER_OF_QUERIES);
	m_CurrentAvailableQuery = 2;

	// Render time of the command buffer
	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools, 0);
	m_CurrentFrameIndex = currentFrameIndex;
}

void VulkanRenderCommandBuffer::End()
{
	VkCommandBuffer commandBuffer;
	commandBuffer = m_CommandBuffers[m_CurrentFrameIndex];

	vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools, 1);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		assert(false);
}

void VulkanRenderCommandBuffer::Submit()
{
	VkDevice device = VulkanDevice::getVulkanDevice();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrameIndex];
	submitInfo.pCommandBuffers = &commandBuffer;

	{
		if (vkQueueSubmit(VulkanDevice::getVulkanContext()->getGraphicsQueue(), 1, &submitInfo,
			m_inFlightFences[m_CurrentFrameIndex]) != VK_SUCCESS)
			assert(false);
		vkWaitForFences(device, 1, &m_inFlightFences[m_CurrentFrameIndex], VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &m_inFlightFences[m_CurrentFrameIndex]);
	}
}

void VulkanRenderCommandBuffer::beginTimeQuery(const std::string& name)
{
	vkCmdWriteTimestamp(m_CurrentCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools, m_CurrentAvailableQuery);
	m_QueryStart[name] = m_CurrentAvailableQuery;
	m_CurrentAvailableQuery += 2;
}

void VulkanRenderCommandBuffer::endTimeQuery(const std::string& name)
{
	if (m_QueryStart.find(name) == m_QueryStart.end())
		return;

	uint32_t startIndex = m_QueryStart[name];
	vkCmdWriteTimestamp(m_CurrentCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools, startIndex + 1);
}

void VulkanRenderCommandBuffer::beginPipelineQuery(const std::string& name, const std::vector<VkQueryPipelineStatisticFlagBits>& statsBits)
{
	if (!m_QueryPoolsPipelineStats.contains(name))
	{
		VkQueryPoolCreateInfo queryPoolInfo = {};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		
		VkQueryPipelineStatisticFlagBits stats;
		for (VkQueryPipelineStatisticFlagBits bit : statsBits)
			queryPoolInfo.pipelineStatistics |= bit;

		queryPoolInfo.queryCount = 1;
		vkCreateQueryPool(VulkanDevice::getVulkanDevice(), &queryPoolInfo, NULL, &m_QueryPoolsPipelineStats[name]);
		m_PipelineStats[name].resize(statsBits.size());
	}

	vkCmdResetQueryPool(m_CurrentCommandBuffer, m_QueryPoolsPipelineStats[name], 0, 1);
	vkCmdBeginQuery(m_CurrentCommandBuffer, m_QueryPoolsPipelineStats[name], 0, 0);
}

void VulkanRenderCommandBuffer::removePipelineQuery(const std::string& name)
{
	if (!m_QueryPoolsPipelineStats.contains(name))
		return;

	vkDestroyQueryPool(VulkanDevice::getVulkanDevice(), m_QueryPoolsPipelineStats[name], nullptr);
	m_QueryPoolsPipelineStats.erase(name);
}

void VulkanRenderCommandBuffer::endPipelineQuery(const std::string& name)
{
	vkCmdEndQuery(m_CurrentCommandBuffer, m_QueryPoolsPipelineStats[name], 0);
}

void VulkanRenderCommandBuffer::fetchPipelineQueries()
{
	for (auto& [name, pool] : m_QueryPoolsPipelineStats)
	{
		uint32_t dataSize = static_cast<uint32_t>(m_PipelineStats[name].size()) * sizeof(uint64_t);
		uint32_t stride = static_cast<uint32_t>(m_PipelineStats[name].size()) * sizeof(uint64_t);
		vkGetQueryPoolResults(
			VulkanDevice::getVulkanDevice(),
			pool,
			0,
			1,
			dataSize,
			m_PipelineStats[name].data(),
			stride,
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	}
}

void VulkanRenderCommandBuffer::fetchTimeQueries()
{
	VkResult res = vkGetQueryPoolResults(VulkanDevice::getVulkanDevice(), m_QueryPools, 0, MAX_NUMBER_OF_QUERIES,
		MAX_NUMBER_OF_QUERIES * 2 * sizeof(uint64_t), m_TimeStamps[m_CurrentFrameIndex].data(), sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT);

	for (uint32_t i = 0; i < m_CurrentAvailableQuery; i += 2)
	{
		uint64_t startTime = m_TimeStamps[m_CurrentFrameIndex][i];
		uint64_t endTime = m_TimeStamps[m_CurrentFrameIndex][i + 1];
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
	if (!m_QueryStart.contains(name))
		return 0.0f;
	uint32_t startIndex = m_QueryStart[name];
	return m_QueryResults[startIndex / 2];
}

std::vector<uint64_t> VulkanRenderCommandBuffer::getPipelineQuery(const std::string& name)
{
	if (!m_PipelineStats.contains(name))
		return {};
	return m_PipelineStats[name];
}
