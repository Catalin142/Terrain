#include <iostream>

#include "Core/Benchmark.h"
#include "VulkanApp.h"


int main()
{
	BEGIN_SESSION("Vulkan_Terrain", "benchmark.json");

	std::shared_ptr<VulkanApp> app = std::make_shared<VulkanApp>("Terrain", 1600, 900);
	app->Run();

	END_SESSION;

	return 0;
}