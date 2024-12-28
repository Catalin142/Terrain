#pragma once
#include <string>
#include <vulkan/vulkan.h>

#define MAXIMUM_NUMBER_OF_SETS_PER_STAGE 8
#define MAXIMUM_ARRAY_ELEMENTS 32

enum class ShaderStage : uint32_t
{
	VERTEX,
	FRAGMENT,
	COMPUTE,
	GEOMETRY,

	NONE,
	ALL,

};

enum class ShaderInputType
{
	UNIFORM_BUFFER,
	UNIFORM_BUFFER_SET,
	STORAGE_BUFFER,
	STORAGE_BUFFER_SET,
	COMBINED_IMAGE_SAMPLER,
	SAMPLER,
	TEXTURE,
	STORAGE_IMAGE,
};

struct ShaderInput
{
	std::string DebugName;

	uint32_t Set;
	uint32_t Count;
	uint32_t Index = 0;
	uint32_t Binding;
	ShaderStage Stage;
	ShaderInputType Type;
};