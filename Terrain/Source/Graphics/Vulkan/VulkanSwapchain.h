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
	void endFrame(VkCommandBuffer commandBuffer);
	void presentFrame();

	VkSwapchainKHR getVkHandle() const { return m_SwapChain; }
	VkRenderPass getRenderPass() const { return m_RenderPass; }

	VkFramebuffer getCurrentFramebuffer() const { return m_Framebuffers[m_ImageIndex]; }
	VkFramebuffer getFramebuffer(uint32_t frameIndex) const { return m_Framebuffers[frameIndex]; }

	VkFence getCurrentFence() const { return m_inFlightFences[currentFrameIndex]; }
	bool getCurrentFenceStatus(uint32_t index);
	VkFence getFence(uint32_t frameIndex) const { return m_inFlightFences[frameIndex]; }

	uint32_t getWidth() { return m_Width; }
	uint32_t getHeight() { return m_Height; }

	uint32_t getImageIndex() { return m_ImageIndex; }
	uint32_t getFramesInFlight() { return framesInFlight; }

public:
	uint32_t currentFrameIndex = 0;
	static inline uint32_t framesInFlight = 1;
	static inline VkExtent2D Extent;

private:
	VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
	VkRenderPass m_RenderPass = VK_NULL_HANDLE;

	VkSurfaceFormatKHR m_SurfaceFormat;

	bool m_vSync = true;

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
	
	uint32_t m_ImageCount = 0;
	uint32_t m_ImageIndex = 0;
	
	VkPresentModeKHR m_PresentMode	= VK_PRESENT_MODE_IMMEDIATE_KHR;
	
	uint32_t m_Width = 0, m_Height = 0;
};