#pragma once
#include <string>
#include <vulkan/vulkan.h>

#define MAXIMUM_NUMBER_OF_SETS_PER_STAGE 8

enum class ShaderStage : uint32_t
{
	VERTEX,
	FRAGMENT,

	NONE
};

enum class ShaderInputType
{
	UNIFORM_BUFFER,
	UNIFORM_BUFFER_SET,
	IMAGE_SAMPLER,
};

struct ShaderInput
{
	std::string DebugName;

	uint32_t Set;
	uint32_t Binding;
	ShaderStage Stage;
	ShaderInputType Type;
};