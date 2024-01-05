#include "window.h"
#include <iostream>

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	Window* wnd = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	wnd->Resized = true;
	wnd->m_Width = width;
	wnd->m_Height = height;
}

Window::Window(uint32_t width, uint32_t height, const std::string& name) : m_Width(width), m_Height(height)
{
	if (!glfwInit())
		std::cerr << "Eraore glfw\n";

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); 

	m_Handle = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_Handle, this);
	glfwSetFramebufferSizeCallback(m_Handle, framebufferResizeCallback);
}

Window::~Window()
{
	glfwDestroyWindow(m_Handle);
	glfwTerminate();
}

void Window::Update()
{
	glfwPollEvents();
}

bool Window::isOpened()
{
	return !glfwWindowShouldClose(m_Handle);
}
