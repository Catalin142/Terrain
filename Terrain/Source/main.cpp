#include <iostream>

#include "VulkanApp.h"

int main()
{
	std::shared_ptr<VulkanApp> app = std::make_shared<VulkanApp>("Terrain", 1600, 900);
	app->Run();

	return 0;
}