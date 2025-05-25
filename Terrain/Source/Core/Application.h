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
	virtual void onDestroy() {};

	uint32_t getWidth() { return m_Window->getWidth(); }
	uint32_t getHeight() { return m_Window->getHeight(); }

	const std::shared_ptr<Window>& getWindow() { return m_Window; }

	static Application* Get() { return m_Instance; }

protected:
	static Application* m_Instance;

	std::shared_ptr<Window> m_Window;
	std::shared_ptr<VulkanDevice> m_Device;
};