#include "VulkanDevice.h"

#include <iostream>
#include <cassert>

#include "GLFW/glfw3.h"


VulkanDevice* VulkanDevice::m_Instance;

const char* getLayerName(GPULayer layer)
{
	switch (layer)
	{
	case GPULayer::VALIDATION:
		return "VK_LAYER_KHRONOS_validation";
	default:
		break;
	}
}

const char* getExtensionName(GPUExtension extension)
{
	switch (extension)
	{
	case GPUExtension::SWAPCHAIN:
		return VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	case GPUExtension::ATOMIC_FLOAT:
		return VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME;
	default:
		break;
	}
}

void enableGPUFeature(VkPhysicalDeviceFeatures& deviceFeatures, VkPhysicalDeviceFeatures supportedFeatures, GPUFeature feature)
{
	static const std::unordered_map<GPUFeature, VkBool32 VkPhysicalDeviceFeatures::*> featureMap = 
	{
	   { GPUFeature::SAMPLER_ANISOTROPY,         &VkPhysicalDeviceFeatures::samplerAnisotropy },
	   { GPUFeature::SAMPLE_RATE_SHADING,        &VkPhysicalDeviceFeatures::sampleRateShading },
	   { GPUFeature::FILL_MODE_NON_SOLID,        &VkPhysicalDeviceFeatures::fillModeNonSolid },
	   { GPUFeature::GEOMETRY_SHADER,            &VkPhysicalDeviceFeatures::geometryShader },
	   { GPUFeature::TESSELLATION_SHADER,        &VkPhysicalDeviceFeatures::tessellationShader },
	   { GPUFeature::WIDE_LINES,                 &VkPhysicalDeviceFeatures::wideLines },
	   { GPUFeature::PIPELINE_STATISTICS_QUERY,  &VkPhysicalDeviceFeatures::pipelineStatisticsQuery },
	};

	auto it = featureMap.find(feature);
	if (it != featureMap.end()) 
	{
		VkBool32 VkPhysicalDeviceFeatures::* feature = it->second;
		if (supportedFeatures.*feature)
			deviceFeatures.*feature = VK_TRUE;
		else
		{
			std::cout << "Feature not supported\n";
		}
	}
}

std::vector<const char*> getLayerList(const std::vector<GPULayer>& requestedLayers)
{
	std::vector<const char*> layers;
	for (GPULayer layer : requestedLayers)
		layers.push_back(getLayerName(layer));

	return layers;
}

std::vector<const char*> getExtensionList(const std::vector<GPUExtension>& requestedExtensions)
{
	std::vector<const char*> extensions;
	for (GPUExtension ext : requestedExtensions)
		extensions.push_back(getExtensionName(ext));

	return extensions;
}

/*
* This function erase the unvailable requested layers
*/
static void checkLayersSupport(std::vector<GPULayer>& requestedLayers)
{
	uint32_t LayerCount = 0;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
	std::vector<VkLayerProperties> layers(LayerCount);
	vkEnumerateInstanceLayerProperties(&LayerCount, layers.data());

#if DEBUG
	std::cout << "\nrequested layers:\n";
	for (const auto& layer : requestedLayers)
		std::cout << "\t" << getLayerName(layer) << "\n";

	std::cout << "\navailable layers:\n";
	for (const auto& layer : layers)
		std::cout << "\t" << layer.layerName << "\n";
#endif

	for (GPULayer layer : requestedLayers)
	{
		const char* layerName = getLayerName(layer);
		bool layerFound = false;
		for (const auto& layer : layers)
			if (strcmp(layerName, layer.layerName))
			{
				layerFound = true;
				break;
			}

		if (!layerFound)
			assert(true, "Layer not found!");
	}
}

VulkanDevice::VulkanDevice(const std::shared_ptr<Window>& window, const InstanceProperties& instanceProps)
{
	createInstance(instanceProps);

	if (glfwCreateWindowSurface(m_VulkanPlatform.Instance, window->getHandle(), nullptr, &m_VulkanPlatform.windowSurface) != VK_SUCCESS)
	{
		std::cout << "Eroare createWindowSurface\n";
		assert(false);
	}

	pickPhysicalDevice();
	createLogicalDevice(instanceProps);
	
	m_Instance = this;
}

VulkanDevice::~VulkanDevice()
{
	for(auto& [tId, cmdPool] : m_CommandPool)
		vkDestroyCommandPool(m_VulkanPlatform.logicalDevice, cmdPool, nullptr);

	vkDestroyDevice(m_VulkanPlatform.logicalDevice, nullptr);
	vkDestroySurfaceKHR(m_VulkanPlatform.Instance, m_VulkanPlatform.windowSurface, nullptr);
	vkDestroyInstance(m_VulkanPlatform.Instance, nullptr);
}

VkCommandPool VulkanDevice::getGraphicsCommandPool()
{
	std::map<std::thread::id, VkCommandPool>& commandPool = m_Instance->m_CommandPool;

	if (commandPool.find(std::this_thread::get_id()) == commandPool.end())
	{
		VkCommandPool vkCommandPool;

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = VulkanDevice::getVulkanContext()->getGraphicsFamilyIndex();

		if (vkCreateCommandPool(VulkanDevice::getVulkanDevice(), &poolInfo, nullptr, &vkCommandPool) != VK_SUCCESS)
		{
			std::cout << "eroare CommandPool\n";
			assert(false);
		}
		commandPool[std::this_thread::get_id()] = vkCommandPool;
	}

	return commandPool[std::this_thread::get_id()];
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

	std::vector<const char*> layers = getLayerList(instanceProps.requestedLayers);
	createInfo.ppEnabledLayerNames = layers.data();

	VkResult res = vkCreateInstance(&createInfo, nullptr, &m_VulkanPlatform.Instance);
	if (res != VK_SUCCESS)
	{
		std::cerr << "Eroare la instance\n";
		assert(false);
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
	if (glfwCreateWindowSurface(m_VulkanPlatform.Instance, window->getHandle(), nullptr, &m_VulkanPlatform.windowSurface) != VK_SUCCESS)
	{
		std::cout << "Eroare createWindowSurface\n";
		assert(false);
	}
}

void VulkanDevice::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_VulkanPlatform.Instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_VulkanPlatform.Instance, &deviceCount, physicalDevices.data());

	m_VulkanPlatform.physicalDevice = physicalDevices[0];

	getGPUQueues(m_VulkanPlatform.physicalDevice);
	vkGetPhysicalDeviceProperties(m_VulkanPlatform.physicalDevice, &m_PhysicalDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(m_VulkanPlatform.physicalDevice, &m_PhysicalDeviceMemoryProperties);
	vkGetPhysicalDeviceFeatures(m_VulkanPlatform.physicalDevice, &m_PhysicalDeviceFeatures);
	GPUName = m_PhysicalDeviceProperties.deviceName;

#if DEBUG
	std::cout << "GPU: " << m_PhysicalDeviceProperties.deviceName << "\n";
	uint64_t memory = 0;
	for (uint32_t i = 0; i < m_PhysicalDeviceMemoryProperties.memoryHeapCount; i++)
		if (m_PhysicalDeviceMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			memory = memory + m_PhysicalDeviceMemoryProperties.memoryHeaps[i].size;

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

	// Search for separate compute queue
	for (size_t i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT && m_ComputeQueue.familyIndex == INVALID_VK_INDEX &&
			(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
		{
			GPUQueue queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_ComputeQueue = queueBundle;
			availableQueues++;
			break;
		}
	}
	for (size_t i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT && m_ComputeQueue.familyIndex == INVALID_VK_INDEX)
		{
			GPUQueue queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_ComputeQueue = queueBundle;
			availableQueues++;
		}

		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && m_GraphicsQueue.familyIndex == INVALID_VK_INDEX)
		{
			GPUQueue queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_GraphicsQueue = queueBundle;
			availableQueues++;
		}

		if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT && m_TransferQueue.familyIndex == INVALID_VK_INDEX)
		{
			GPUQueue queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_TransferQueue = queueBundle;
			availableQueues++;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, (uint32_t)i, m_VulkanPlatform.windowSurface, &presentSupport);

		if (presentSupport && m_PresentQueue.familyIndex == INVALID_VK_INDEX)
		{
			GPUQueue queueBundle;
			queueBundle.familyIndex = (uint32_t)i;
			m_PresentQueue = queueBundle;
			availableQueues++;
		}
	}

	return availableQueues;
}

void VulkanDevice::createLogicalDevice(const InstanceProperties& instanceProps)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { getGraphicsFamilyIndex(), getPresentFamilyIndex(), getComputeFamilyIndex()};

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
	VkPhysicalDeviceFeatures supportedFeatures{};
	vkGetPhysicalDeviceFeatures(m_VulkanPlatform.physicalDevice, &supportedFeatures);

	for (GPUFeature feature : instanceProps.requestedFeatures)
		enableGPUFeature(deviceFeatures, supportedFeatures, feature);

	VkDeviceCreateInfo deviceCreateInfo{};

	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures{};
	VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT atomicFloat2Features{};

	bool enableFloat	= false;
	bool enableFloat2	= false;

	const void* next = deviceCreateInfo.pNext;
		
	for (GPUFeatureEXT feature : instanceProps.requestedFeaturesEXT)
	{
		switch (feature)
		{
		case GPUFeatureEXT::ATOMICS_FLOAT32:
			atomicFloatFeatures.shaderBufferFloat32Atomics = VK_TRUE;
			enableFloat = true;
			break;

		case GPUFeatureEXT::ATOMIC_ADD_FLOAT32:
			atomicFloatFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;
			enableFloat = true;
			break;

		case GPUFeatureEXT::ATOMIC_MIN_MAX_FLOAT32_2:
			atomicFloat2Features.shaderBufferFloat32AtomicMinMax = VK_TRUE;
			enableFloat2 = true;
			break;

		default:
			assert(false, "Implement this");
			break;
		}
	}

	if (enableFloat)
	{
		next = &atomicFloatFeatures;
		next = atomicFloatFeatures.pNext;
	}

	if (enableFloat2)
	{
		next = &atomicFloat2Features;
		next = atomicFloat2Features.pNext;
	}

	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceProps.requestedExtensions.size());

	std::vector<const char*> extensions = getExtensionList(instanceProps.requestedExtensions);
	deviceCreateInfo.ppEnabledExtensionNames = extensions.data();

	deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceProps.requestedLayers.size());

	std::vector<const char*> layers = getLayerList(instanceProps.requestedLayers);
	deviceCreateInfo.ppEnabledLayerNames = layers.data();

	if (vkCreateDevice(m_VulkanPlatform.physicalDevice, &deviceCreateInfo, nullptr, &m_VulkanPlatform.logicalDevice) != VK_SUCCESS)
	{
		std::cerr << "Eraore creare device\n";
		assert(false);
	}

	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_GraphicsQueue.familyIndex, 0, &m_GraphicsQueue.Handle);
	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_PresentQueue.familyIndex, 0, &m_PresentQueue.Handle);
	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_TransferQueue.familyIndex, 0, &m_TransferQueue.Handle);
	vkGetDeviceQueue(m_VulkanPlatform.logicalDevice, m_ComputeQueue.familyIndex, 0, &m_ComputeQueue.Handle);
}
