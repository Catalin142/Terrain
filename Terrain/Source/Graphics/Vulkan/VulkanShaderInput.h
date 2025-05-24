#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <cassert>

#define MAXIMUM_NUMBER_OF_SETS_PER_STAGE 8
#define MAXIMUM_ARRAY_ELEMENTS 32

enum class ShaderStage : uint32_t
{
	VERTEX,
	GEOMETRY,
	TESSELLATION_CONTROL,
	TESSELLATION_EVALUATION,
	FRAGMENT,
	
	COMPUTE,

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


static VkShaderStageFlagBits getStage(const ShaderStage& stage)
{
	switch (stage)
	{
	case ShaderStage::VERTEX:						return VK_SHADER_STAGE_VERTEX_BIT;
	case ShaderStage::GEOMETRY:						return VK_SHADER_STAGE_GEOMETRY_BIT;
	case ShaderStage::TESSELLATION_CONTROL:			return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	case ShaderStage::TESSELLATION_EVALUATION:		return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	case ShaderStage::FRAGMENT:						return VK_SHADER_STAGE_FRAGMENT_BIT;
	case ShaderStage::COMPUTE:						return VK_SHADER_STAGE_COMPUTE_BIT;
	case ShaderStage::NONE:							assert(false); break;
	default:
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}

	return VK_SHADER_STAGE_ALL;
}

static VkDescriptorType getInputType(const ShaderInputType& type)
{
	switch (type)
	{
	case ShaderInputType::UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case ShaderInputType::UNIFORM_BUFFER_SET: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case ShaderInputType::STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case ShaderInputType::STORAGE_BUFFER_SET: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case ShaderInputType::COMBINED_IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case ShaderInputType::TEXTURE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case ShaderInputType::SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
	case ShaderInputType::STORAGE_IMAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	}

	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}