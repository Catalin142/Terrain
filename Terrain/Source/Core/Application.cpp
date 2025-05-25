#include "Application.h"

#include "Graphics/Vulkan/VulkanShader.h"

#include "Instrumentor.h"

float Time::deltaTime = 0;
Application* Application::m_Instance;

Application::Application(const std::string& title, uint32_t width, uint32_t height)
{
	m_Window = std::make_shared<Window>(title, width, height);
	InstanceProperties props = getDefaultInstanceProperties();
	m_Device = std::make_shared<VulkanDevice>(m_Window, props);

	m_Instance = this;
}

Application::~Application()
{
	ShaderManager::Clear();
}

void Application::Run()
{
	onCreate();

	float begTime = (float)glfwGetTime();
	float endTime = begTime;

	while (m_Window->isOpened())
	{
		Instrumentor::Get().beginTimer("_TotalTime");

		if (m_Window->Resized)
		{
			int width = 0, height = 0;
			glfwGetFramebufferSize(m_Window->getHandle(), &width, &height);
			while (width == 0 || height == 0) {
				glfwGetFramebufferSize(m_Window->getHandle(), &width, &height);
				glfwWaitEvents();
			}

			m_Window->Resized = false;

			onResize();
		}

		begTime = (float)glfwGetTime();
		Time::deltaTime = (begTime - endTime);
		endTime = begTime;

		m_Window->Update();

		Instrumentor::Get().beginTimer("_Update");
		onUpdate();
		Instrumentor::Get().endTimer("_Update");

		Instrumentor::Get().endTimer("_TotalTime");
	}

	vkDeviceWaitIdle(m_Device->getLogicalDevice());
	onDestroy();
}
