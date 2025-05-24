#pragma once

#include <vector>
#include <array>
#include <string>
#include <memory>
#include <set>
#include <unordered_map>
#include <thread>
#include <map>
#include <mutex>

#include "Core/Window.h"
#include <vulkan/vulkan.h>

#define INVALID_VK_INDEX 0xFFFFFFFF

enum class GPUExtension
{
	SWAPCHAIN,
	ATOMIC_FLOAT,
};

enum class GPULayer
{
	VALIDATION,
};

enum class GPUFeature
{
	SAMPLER_ANISOTROPY,
	SAMPLE_RATE_SHADING,
	FILL_MODE_NON_SOLID,
	GEOMETRY_SHADER,
	TESSELLATION_SHADER,
	WIDE_LINES,
	PIPELINE_STATISTICS_QUERY
};

enum class GPUFeatureEXT
{
	ATOMICS_FLOAT32,
	ATOMIC_ADD_FLOAT32,
	ATOMIC_MIN_MAX_FLOAT32_2
};

struct InstanceProperties
{
	std::vector<GPUExtension> requestedExtensions;
	std::vector<GPULayer> requestedLayers;
	std::vector<GPUFeature> requestedFeatures;
	std::vector<GPUFeatureEXT> requestedFeaturesEXT;
};

static InstanceProperties getDefaultInstanceProperties()
{
	InstanceProperties defaultProps;

	defaultProps.requestedExtensions = { GPUExtension::SWAPCHAIN, GPUExtension::ATOMIC_FLOAT };
	defaultProps.requestedLayers = { GPULayer::VALIDATION };
	defaultProps.requestedFeatures = { 
		GPUFeature::SAMPLER_ANISOTROPY, GPUFeature::SAMPLE_RATE_SHADING,
		GPUFeature::FILL_MODE_NON_SOLID, GPUFeature::GEOMETRY_SHADER,
		GPUFeature::TESSELLATION_SHADER, GPUFeature::WIDE_LINES,
		GPUFeature::PIPELINE_STATISTICS_QUERY
	};
	defaultProps.requestedFeaturesEXT = {};

	return defaultProps;
}

struct GPUQueue
{
	VkQueue Handle				= VK_NULL_HANDLE;
	uint32_t familyIndex		= INVALID_VK_INDEX;
};

struct VulkanPlatform
{
	VkInstance Instance					= VK_NULL_HANDLE;
	VkSurfaceKHR windowSurface			= VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice		= VK_NULL_HANDLE;
	VkDevice logicalDevice				= VK_NULL_HANDLE;
};

struct VkSurfaceCapabilitiesKHR;

class VulkanDevice
{
public:
	VulkanDevice(const std::shared_ptr<Window>& window, const InstanceProperties& gpuProps = getDefaultInstanceProperties());
	~VulkanDevice();

	inline VkInstance getInstance()	const					{ return m_VulkanPlatform.Instance; }
	inline VkSurfaceKHR getWindowSurface() const			{ return m_VulkanPlatform.windowSurface; }
	inline VkPhysicalDevice getPhysicalDevice() const		{ return m_VulkanPlatform.physicalDevice; }
	inline VkDevice getLogicalDevice() const				{ return m_VulkanPlatform.logicalDevice; }

	inline VkQueue getGraphicsQueue() const					{ return m_GraphicsQueue.Handle; }
	inline VkQueue getPresentQueue() const					{ return m_PresentQueue.Handle; }
	inline VkQueue getComputeQueue() const					{ return m_ComputeQueue.Handle; }
																	 
	inline uint32_t getGraphicsFamilyIndex() const			{ return m_GraphicsQueue.familyIndex; }
	inline uint32_t getPresentFamilyIndex() const			{ return m_PresentQueue.familyIndex; }
	inline uint32_t getComputeFamilyIndex() const			{ return m_ComputeQueue.familyIndex; }

	VkPhysicalDeviceLimits getPhysicalDeviceLimits() const			{ return m_PhysicalDeviceProperties.limits; }
	VkPhysicalDeviceProperties getPhysicalDeviceProperties() const	{ return m_PhysicalDeviceProperties; }

public:
	static VulkanDevice* getVulkanContext()
	{
		return m_Instance;
	}

	static VkDevice getVulkanDevice() 
	{
		return m_Instance->m_VulkanPlatform.logicalDevice;
	}

	static VkCommandPool getGraphicsCommandPool();

	std::string GPUName;

private:
	static VulkanDevice* m_Instance;

	void createInstance(InstanceProperties gpuProps);
	void createWindowSurface(const std::shared_ptr<Window>& window);
	void pickPhysicalDevice();
	void createLogicalDevice(const InstanceProperties& gpuProps);

	uint32_t getGPUQueues(VkPhysicalDevice gpu);

private:
	VulkanPlatform m_VulkanPlatform;

	GPUQueue m_GraphicsQueue;
	GPUQueue m_PresentQueue;
	GPUQueue m_TransferQueue;
	GPUQueue m_ComputeQueue;

	std::map<std::thread::id, VkCommandPool> m_CommandPool;

	VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures;
	VkPhysicalDeviceProperties m_PhysicalDeviceProperties; 
	VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;

	VkPhysicalDeviceProperties m_GPUProperties{};
	VkPhysicalDeviceMemoryProperties m_VRAMProperties{};
};