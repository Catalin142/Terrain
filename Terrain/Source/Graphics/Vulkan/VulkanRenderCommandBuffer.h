#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <vulkan/vulkan.h>

class VulkanRenderCommandBuffer
{
public:
	VulkanRenderCommandBuffer(uint32_t count);
	~VulkanRenderCommandBuffer();

	void Begin(uint32_t frameIndex);
	void End();
	void Submit();

	VkCommandBuffer getCommandBuffer(uint32_t index) {
		return m_CommandBuffers[index];
	}

	void beginTimeQuery(const std::string& name);
	void endTimeQuery(const std::string& name);
	
	void beginPipelineQuery(const std::string& name, const std::vector<VkQueryPipelineStatisticFlagBits>& statsBits);
	void removePipelineQuery(const std::string& name);
	void endPipelineQuery(const std::string& name);

	void fetchPipelineQueries();
	void fetchTimeQueries();

	float getCommandBufferTime();
	float getTime(const std::string& name);

	std::vector<uint64_t> getPipelineQuery(const std::string& name);

private:
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_CommandBuffers;
	VkCommandBuffer m_CurrentCommandBuffer = VK_NULL_HANDLE;
	std::vector<VkFence> m_inFlightFences;
	uint32_t m_CurrentFrameIndex = 0;

	VkQueryPool m_QueryPools;
	std::vector<std::vector<uint64_t>> m_TimeStamps;
	std::unordered_map<std::string, uint32_t> m_QueryStart;
	std::vector<float> m_QueryResults;
	uint32_t m_CurrentAvailableQuery = 0;

	std::unordered_map<std::string, std::vector<uint64_t>> m_PipelineStats;
	std::unordered_map<std::string, VkQueryPool> m_QueryPoolsPipelineStats;
};