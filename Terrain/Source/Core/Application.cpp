#include "Application.h"

#include "Graphics/VulkanRenderer.h"

float Time::deltaTime = 0;
Application* Application::m_Instance;

Application::Application(const std::string& title, uint32_t width, uint32_t height)
{
	m_Window = std::make_shared<Window>(title, width, height);
	InstanceProperties props = getDefaultInstanceProperties();
	m_Device = std::make_shared<VulkanDevice>(m_Window, props);

	m_Swapchain = std::make_shared<VulkanSwapchain>();
	m_Swapchain->Initialize();
	m_Swapchain->Create(width, height);

	VulkanRenderer::Initialize(m_Swapchain);

	m_ImguiLayer = std::make_shared<VulkanImgui>();
	m_ImguiLayer->Initialize(m_Window);

	m_Instance = this;
}

Application::~Application()
{
	m_ImguiLayer->Destroy();
	ShaderManager::Clear();
}

void Application::Run()
{
	onCreate();

	float begTime = glfwGetTime();
	float endTime = begTime;

	while (m_Window->isOpened())
	{
		if (m_Window->Resized)
		{
			int width = 0, height = 0;
			glfwGetFramebufferSize(m_Window->getHandle(), &width, &height);
			while (width == 0 || height == 0) {
				glfwGetFramebufferSize(m_Window->getHandle(), &width, &height);
				glfwWaitEvents();
			}

			m_Swapchain->onResize(width, height);
			m_Window->Resized = false;

			onResize();
		}

		begTime = glfwGetTime();
		Time::deltaTime = (begTime - endTime);
		endTime = begTime;

		m_Window->Update();

		preFrame();
		m_Swapchain->beginFrame();
		onUpdate();
		
		m_Swapchain->endFrame();
		postFrame();
	}

	onDestroy();
	VulkanRenderer::Destroy();
	vkDeviceWaitIdle(m_Device->getLogicalDevice());
}
