#pragma once

#include <vector>
#include <array>
#include <string>
#include <memory>
#include <set>
#include <unordered_map>

#include "Core/Window.h"
#include <vulkan/vulkan.h>

#define INVALID_VK_INDEX 0xFFFFFFFF

struct InstanceProperties
{
	VkPhysicalDeviceType GPUType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
	std::vector<const char*> requestedExtensions;
	std::vector<const char*> requestedLayers;
};

// JUST GRAPHICS AND PRESENT QUEUE SUPPORTED
static InstanceProperties getDefaultInstanceProperties()
{
	InstanceProperties default;
	default.GPUType				= VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	default.requestedExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#if DEBUG
	default.requestedLayers		= { "VK_LAYER_KHRONOS_validation" };
#endif

	return default;
}

/*
* Holds information specific to the swapchain based on the GPU used
*/
struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

/*
* Holds information releated to a Queue
*/
struct QueueBundle
{
	VkQueue handle				= VK_NULL_HANDLE;
	uint32_t familyIndex		= INVALID_VK_INDEX;
};

/*
* A collection of vulkan handles
*/
struct VulkanPlatform
{
	VkInstance Instance			= VK_NULL_HANDLE;
	VkSurfaceKHR WindowSurface	= VK_NULL_HANDLE;
	VkPhysicalDevice GPU		= VK_NULL_HANDLE;
	VkDevice logicalDevice		= VK_NULL_HANDLE;
};

class VulkanCommandPool
{
public:
	VulkanCommandPool();
	~VulkanCommandPool();

	VkCommandPool getGraphicsCommandPool() { return m_GraphicsCommandPool; }

private:
	VkCommandPool m_GraphicsCommandPool;
};

class VulkanDevice
{
public:
	VulkanDevice(const std::shared_ptr<Window>& window, const InstanceProperties& gpuProps = getDefaultInstanceProperties());
	~VulkanDevice();

	bool supportsMSAASamples(uint32_t sampleNumber);
	VkSampleCountFlagBits getMaximumMSAASamples();

	inline VkInstance getInstance()	const			{ return m_VulkanPlatform.Instance; }
	inline VkSurfaceKHR getWindowSurface() const	{ return m_VulkanPlatform.WindowSurface; }
	inline VkPhysicalDevice getGPU() const			{ return m_VulkanPlatform.GPU; }
	inline VkDevice getLogicalDevice() const		{ return m_VulkanPlatform.logicalDevice; }

	inline VkQueue getGraphicsQueue() const			{ return m_graphicsQueue.handle; }
	inline VkQueue getPresentQueue() const			{ return m_presentQueue.handle; }
															 
	inline uint32_t getGraphicsFamilyIndex() const	{ return m_graphicsQueue.familyIndex; }
	inline uint32_t getPresentFamilyIndex() const	{ return m_presentQueue.familyIndex; }

	VkPhysicalDeviceLimits getPhysicalDeviceLimits() const { return m_physicalDeviceProperties.limits; }

	inline SwapchainSupportDetails getSwapchainCapabilities() { return getSwapchainDetails(m_VulkanPlatform.GPU); }

	static VulkanDevice* getVulkanContext()
	{
		return m_Instance;
	}

	static VkDevice getVulkanDevice() 
	{
		return m_Instance->m_VulkanPlatform.logicalDevice;
	}

	static VkCommandPool getGraphicsCommandPool()
	{
		return m_Instance->m_CommandPool->getGraphicsCommandPool();
	}

private:
	static VulkanDevice* m_Instance;

	void createInstance(InstanceProperties gpuProps);
	void createWindowSurface(const std::shared_ptr<Window>& window);

	/*
	* Based on the scores of the GPUS, picks the best options of them all
	*/
	void pickPhysicalDevice(const InstanceProperties& gpuProps);

	/*
	* Returns a list of all the unvailable extensions that are specified in the InstanceProperties
	*/
	std::set<std::string> getUnavailableExtensions(VkPhysicalDevice gpu, const InstanceProperties& gpuProps);
	SwapchainSupportDetails getSwapchainDetails(VkPhysicalDevice gpu);

	void createLogicalDevice(const InstanceProperties& gpuProps);

	uint32_t VulkanDevice::getGPUQueues(VkPhysicalDevice gpu);

private:
	VulkanPlatform m_VulkanPlatform;

	QueueBundle m_graphicsQueue;
	QueueBundle m_presentQueue;
	QueueBundle m_transferQueue;

	std::shared_ptr<VulkanCommandPool> m_CommandPool;

	VkPhysicalDeviceFeatures m_physicalDeviceFeatures;
	VkPhysicalDeviceProperties m_physicalDeviceProperties; 
	VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;

	VkPhysicalDeviceProperties m_GPUProperties{};
	VkPhysicalDeviceMemoryProperties m_VRAMProperties{};
};