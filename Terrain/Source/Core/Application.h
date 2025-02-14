#pragma once

#include "Window.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanSwapchain.h"
#include "Graphics/Vulkan/VulkanImgui.h"

#include <memory>

struct Time
{
	static float deltaTime;
};

class Application
{
public:
	Application(const std::string& title, uint32_t width, uint32_t height); 
	~Application();

	void Run();

	virtual void onCreate() = 0;
	virtual void onResize() {};
	virtual void onUpdate() = 0;
	virtual void onDestroy() = 0;
	virtual void postFrame() {}
	virtual void preFrame() {}

	uint32_t getWidth() { return m_Window->getWidth(); }
	uint32_t getHeight() { return m_Window->getHeight(); }

	VkExtent2D getExtent() { return m_Swapchain->getExtent(); }

	const std::shared_ptr<Window>& getWindow() { return m_Window; }

	static Application* Get() { return m_Instance; }

	void beginImGuiFrame() { m_ImguiLayer->beginFrame(); }
	void endImGuiFrame() { m_ImguiLayer->endFrame(); }

protected:
	static Application* m_Instance;

	std::shared_ptr<Window> m_Window;
	std::shared_ptr<VulkanDevice> m_Device;
	std::shared_ptr<VulkanSwapchain> m_Swapchain;
	std::shared_ptr<VulkanImgui> m_ImguiLayer;
};