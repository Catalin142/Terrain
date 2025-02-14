#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <vulkan/vulkan.h>

class VulkanRenderCommandBuffer
{
public:
	VulkanRenderCommandBuffer(uint32_t count);
	VulkanRenderCommandBuffer(bool swapchain);
	~VulkanRenderCommandBuffer();

	void Begin();
	void End();
	void Submit();

	VkCommandBuffer getCurrentCommandBuffer() { return m_CurrentCommandBuffer; }
	VkCommandBuffer getCommandBuffer(uint32_t index) {
		return m_CommandBuffers[index];
	}

	void beginQuery(const std::string& name);
	void endQuery(const std::string& name);
	void queryResults();

	float getCommandBufferTime();
	float getTime(const std::string& name);
	bool getCurrentBufferStatus();

private:
	VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_CommandBuffers;
	VkCommandBuffer m_CurrentCommandBuffer = VK_NULL_HANDLE;
	std::vector<VkFence> m_inFlightFences;

	std::vector<VkQueryPool> m_QueryPools;
	std::vector<std::vector<uint64_t>> m_TimeStamps;
	std::unordered_map<std::string, uint32_t> m_QueryStart;
	std::vector<float> m_QueryResults;
	uint32_t m_CurrentAvailableQuery = 0;
	uint32_t m_QueryFrame = 0;

	bool m_OwnedBySwapchain = false;
};