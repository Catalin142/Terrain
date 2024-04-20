#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanSwapchain
{
public:
	VulkanSwapchain() = default;
	~VulkanSwapchain();

	void Initialize();

	void Create(uint32_t width, uint32_t height);
	void onResize(uint32_t width, uint32_t height);
	void Destroy();

	void beginFrame();
	void endFrame();
	void presentFrame();

	VkSwapchainKHR getVkHandle() const { return m_SwapChain; }
	VkRenderPass getRenderPass() const { return m_RenderPass; }

	VkCommandBuffer getCurrentCommandBuffer() const { return m_CommandBuffers[m_currentFrameIndex].commandBuffer; }
	VkCommandBuffer getCommandBuffer(uint32_t frameIndex) const { return m_CommandBuffers[frameIndex].commandBuffer; }

	VkFramebuffer getCurrentFramebuffer() const { return m_Framebuffers[m_ImageIndex]; }
	VkFramebuffer getFramebuffer(uint32_t frameIndex) const { return m_Framebuffers[frameIndex]; }

	VkSemaphore getImageAvailableSemaphore(uint32_t frameIndex) { return m_Semaphores[frameIndex].imageAvailableSemaphore; }
	VkSemaphore getRenderFinishedSemaphore(uint32_t frameIndex) { return m_Semaphores[frameIndex].renderFinishedSemaphore; }

	VkFence getCurrentFence() const { return m_inFlightFences[m_currentFrameIndex]; }
	VkFence getFence(uint32_t frameIndex) const { return m_inFlightFences[frameIndex]; }

	VkExtent2D getExtent() { return m_Extent; }

	uint32_t getWidth() { return m_Width; }
	uint32_t getHeight() { return m_Height; }

	uint32_t getCurrentFrame() { return m_currentFrameIndex; }
	uint32_t getImageIndex() { return m_ImageIndex; }
	uint32_t getFramesInFlight() { return m_FramesInFlight; }

private:
	VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;

	VkSurfaceFormatKHR m_SurfaceFormat;
	VkExtent2D m_Extent;

	bool m_vSync = true;

	// For each frame
	struct SwapchainCommandBuffer
	{
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;
	};
	std::vector<SwapchainCommandBuffer> m_CommandBuffers;

	struct SwapchainSemaphore {
		VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	};
	std::vector<SwapchainSemaphore> m_Semaphores;

	struct SwapchainImage
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView ImageView = VK_NULL_HANDLE;
	};
	std::vector<SwapchainImage> m_ColorImages;

	std::vector<VkFramebuffer> m_Framebuffers;
	std::vector<VkFence> m_inFlightFences;
	
	uint32_t m_currentFrameIndex;

	uint32_t m_ImageCount = 0;
	uint32_t m_ImageIndex = 0;
	
	VkPresentModeKHR m_PresentMode	= VK_PRESENT_MODE_IMMEDIATE_KHR;
	
	uint32_t m_Width = 0, m_Height = 0;
	uint32_t m_FramesInFlight = 1;
};