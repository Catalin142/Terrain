#pragma once
#include <cstdint>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

class Window
{
	friend static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

public:
	Window(const std::string& name, uint32_t width, uint32_t height);
	~Window();

	GLFWwindow* getHandle() { return m_Handle; }

	uint32_t getWidth() { return m_Width; }
	uint32_t getHeight() { return m_Height; }

	void Update();
	bool isOpened();

public:
	bool Resized = false;

private:
	GLFWwindow* m_Handle;
	uint32_t m_Width, m_Height;
};