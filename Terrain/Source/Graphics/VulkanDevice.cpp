#include "VulkanDevice.h"

#include <iostream>

#include "GLFW/glfw3.h"


VulkanDevice* VulkanDevice::m_Instance;

/*
* This function erase the unvailable requested layers
*/
static void checkLayersSupport(std::vector<const char*>& requestedLayers)
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> layers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

#if DEBUG
	std::cout << "\nrequested layers:\n";
	for (const auto& layer : requestedLayers)
		std::cout << "\t" << layer << "\n";

	std::cout << "\navailable layers:\n";
	for (const auto& layer : layers)
		std::cout << "\t" << layer.layerName << "\n";
#endif

	for (auto it = requestedLayers.begin(); it != requestedLayers.end(); it++)
	{
		const char* layerName = *it;
		bool layerFound = false;
		for (const auto& layer : layers)
			if (strcmp(layerName, layer.layerName))
			{
				layerFound = true;
				break;
			}

		if (!layerFound)
			requestedLayers.erase(it);
	}
}

VulkanDevice::VulkanDevice(const std::shared_ptr<Window>& window, const InstanceProperties& instanceProps)
{
	createInstance(instanceProps);
	createWindowSurface(window);
	pickPhysicalDevice(instanceProps);
	createLogicalDevice(instanceProps);
	
	m_Instance = this;
	m_CommandPool = std::make_shared<VulkanCommandPool>();
}

VulkanDevice::~VulkanDevice()
{
	m_CommandPool.reset();
	vkDestroyDevice(m_VulkanPlatform.logicalDevice, nullptr);
	vkDestroySurfaceKHR(m_VulkanPlatform.Instance, m_VulkanPlatform.WindowSurface, nullptr);
	vkDestroyInstance(m_VulkanPlatform.Instance, nullptr);
}

bool VulkanDevice::supportsMSAASamples(uint32_t sampleNumber)
{
	VkSampleCountFlags counts = m_physicalDeviceProperties.limits.framebufferColorSampleCounts & m_physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	return counts & sampleNumber;
}

VkSampleCountFlagBits VulkanDevice::getMaximumMSAASamples()
{
	VkSampleCountFlags counts = m_physicalDeviceProperties.limits.framebufferColorSampleCounts & m_physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
	if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
	if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
	return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanDevice::createInstance(InstanceProperties instanceProps)
{
	checkLayersSupport(instanceProps.requestedLayers);

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> enabledExtension(glfwExtensions, glfwExtensions + glfwExtensionCount);

	createInfo.enabledExtensionCount = (uint32_t)enabledExtension.size();
	createInfo.ppEnabledExtensionNames = enabledExtension.data();

	createInfo.ppEnabledLayerNames = 0;
	createInfo.pNext = nullptr;

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
	createInfo.enabledLayerCount = (uint32_t)instanceProps.requestedLayers.size();
	createInfo.ppEnabledLayerNames = instanceProps.requestedLayers.data();

	VkResult res = vkCreateInstance(&createInfo, nullptr, &m_VulkanPlatform.Instance);
	if (res != VK_SUCCESS)
	{
		std::cerr << "Eroare la instance\n";
		throw(false);
	}

#if DEBUG
	uint32_t count;
	vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> extensionProps(count);
	vkEnumerateInstanceExtensionProperties(nullptr, &count, extensionProps.data());

	std::cout << "\navailable extensions:\n";
	for (const auto& ext : extensionProps)
		std::cout << "\t" << ext.extensionName << "\n";
#endif
}

void VulkanDevice::createWindowSurface(const std::shared_ptr<Window>& window)
{
	if (glfwCreateWindowSurface(m_VulkanPlatform.Instance, window->getHandle(), nullptr, &m_VulkanPlatform.WindowSurface) != VK_SUCCESS)
	{
		std::cout << "Eroare createWindowSurface\n";
		throw(false);
	}
}

void VulkanDevice::pickPhysicalDevice(const InstanceProperties& instanceProps)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_VulkanPlatform.Instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_VulkanPlatform.Instance, &deviceCount, physicalDevices.data());

	m_VulkanPlatform.GPU = physicalDevices[0];

	getGPUQueues(m_VulkanPlatform.GPU);
	vkGetPhysicalDeviceProperties(m_VulkanPlatform.GPU, &m_physicalDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(m_VulkanPlatform.GPU, &m_physicalDeviceMemoryProperties);
	vkGetPhysicalDeviceFeatures(m_VulkanPlatform.GPU, &m_physicalDeviceFeatures);

#if DEBUG
	std::cout << "GPU: " << m_physicalDeviceProperties.deviceName << "\n";
	uint64_t memory = 0;
	for (uint32_t i = 0; i < m_physicalDeviceMemoryProperties.memoryHeapCount; i++)
		if (m_physicalDeviceMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			memory = memory + m_physicalDeviceMemoryProperties.memoryHeaps[i].size;

	std::cout << "VRAM: " << memory / 1024 / 1024 << " MB\n";
#endif
}

uint32_t VulkanDevice::getGPUQueues(VkPhysicalDevice gpu)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

	uint32_t availableQueues = 0;

	for (size_t i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && m_graphicsQueue.familyIndex == INVALID_VK_INDEX)
		{
			QueueBundle queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_graphicsQueue = queueBundle;
			availableQueues++;
		}

		if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT && m_transferQueue.familyIndex == INVALID_VK_INDEX)
		{
			QueueBundle queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_transferQueue = queueBundle;
			availableQueues++;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, (uint32_t)i, m_VulkanPlatform.WindowSurface, &presentSupport);

		if (presentSupport && m_presentQueue.familyIndex == INVALID_VK_INDEX)
		{
			QueueBundle queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_presentQueue = queueBundle;
			availableQueues++;
		}
	}

	return availableQueues;
}

std::set<std::string> VulkanDevice::getUnavailableExtensions(VkPhysicalDevice gpu, const InstanceProperties& instanceProps)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(instanceProps.requestedExtensions.begin(), instanceProps.requestedExtensions.end());

	for (const auto& extension : availableExtensions) 
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions;
}

SwapchainSupportDetails VulkanDevice::getSwapchainDetails(VkPhysicalDevice gpu)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, m_VulkanPlatform.WindowSurface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_VulkanPlatform.WindowSurface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_VulkanPlatform.WindowSurface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_VulkanPlatform.WindowSurface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_VulkanPlatform.WindowSurface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

void VulkanDevice::createLogicalDevice(const InstanceProperties& instanceProps)
{
	// iau indexu la toate queue rile de care am nevoie

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { getGraphicsFamilyIndex(), getPresentFamilyIndex() };

	float queuePriority = 1.0f;
	for (uint32_t famIndex : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex	= famIndex;
		queueCreateInfo.queueCount			= 1;
		queueCreateInfo.pQueuePriorities	= &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE; // trb sa actvezi feature
	deviceFeatures.sampleRateShading = VK_TRUE;
	deviceFeatures.fillModeNonSolid  = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	// queue
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	// gpu features
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceProps.requestedExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = instanceProps.requestedExtensions.data();

	// in versinile mai noi valorile astea sunt ignoarate de vulkan, e good practice sa le setezi pt backwards compatibility
	deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceProps.requestedLayers.size());
	deviceCreateInfo.ppEnabledLayerNames = instanceProps.requestedLayers.data();

	if (vkCreateDevice(m_VulkanPlatform.GPU, &deviceCreateInfo, nullptr, &m_VulkanPlatform.logicalDevice) != VK_SUCCESS)
	{
		std::cerr << "Eraore creare device\n";
		throw(false);
	}

	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_graphicsQueue.familyIndex, 0, &m_graphicsQueue.handle);
	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_presentQueue.familyIndex, 0, &m_presentQueue.handle);
	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_transferQueue.familyIndex, 0, &m_transferQueue.handle);
}

VulkanCommandPool::VulkanCommandPool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = VulkanDevice::getVulkanContext()->getGraphicsFamilyIndex();

	if (vkCreateCommandPool(VulkanDevice::getVulkanDevice(), &poolInfo, nullptr, &m_GraphicsCommandPool) != VK_SUCCESS)
	{
		std::cout << "eroare CommandPool\n";
		throw(false);
	}
}

VulkanCommandPool::~VulkanCommandPool()
{
	vkDestroyCommandPool(VulkanDevice::getVulkanDevice(), m_GraphicsCommandPool, nullptr);
}
