#include "VulkanShader.h"

#include "VulkanDevice.h"

#include <vector>
#include <fstream>
#include <cassert>
#include <iostream>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>

std::unordered_map<std::string, std::shared_ptr<VulkanShader>> ShaderManager::m_Shaders;

static VkShaderStageFlagBits getStage(const ShaderStage& stage)
{
	switch (stage)
	{
	case ShaderStage::VERTEX:   return VK_SHADER_STAGE_VERTEX_BIT;
	case ShaderStage::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT; break;
	case ShaderStage::NONE:     assert(false); break;
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
	case ShaderInputType::IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}

	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

static std::vector<char> readBinary(const std::string& filepath)
{
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);
	if (!file.good())
	{
		std::cerr << "FIsierul " << filepath << " nu exista\n";
		throw(false);
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

static std::string readFile(const std::string& filepath)
{
	std::ifstream file(filepath);
	if (!file.good())
	{
		std::cerr << "FIsierul " << filepath << " nu exista\n";
		throw(false);
	}

	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string buffer(size, ' ');
	file.seekg(0);
	file.read(&buffer[0], size);

	return buffer;
}

VulkanShaderStage::VulkanShaderStage(ShaderStage stage, const std::string& filepath) : m_Stage(stage)
{
	std::vector<uint32_t> data = VulkanShaderCompiler::compileVulkanShader(stage, filepath);
	m_Input = VulkanShaderCompiler::Reflect(stage, data);

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = data.size() * sizeof(uint32_t);
	createInfo.pCode = data.data();

	if (vkCreateShaderModule(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &m_ShaderModule) != VK_SUCCESS)
	{
		std::cerr << "Eroare shader\n";
		throw(false);
	}
}

VulkanShaderStage::~VulkanShaderStage()
{
	vkDestroyShaderModule(VulkanDevice::getVulkanDevice(), m_ShaderModule, nullptr);
}

VkPipelineShaderStageCreateInfo const VulkanShaderStage::getStageCreateInfo()
{
	VkPipelineShaderStageCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	createInfo.stage = getStage(m_Stage);
	createInfo.module = m_ShaderModule;
	createInfo.pName = "main";

	return createInfo;
}

VulkanShader::~VulkanShader()
{
	for (VkDescriptorSetLayout layout : m_DescriptorSetLayouts)
		vkDestroyDescriptorSetLayout(VulkanDevice::getVulkanDevice(), layout, nullptr);
	m_Stages.clear();
}

void VulkanShader::createDescriptorSetLayouts()
{
	std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings = getDescriptorSetLayoutBindings();

	for (auto& [set, descLayoutBindings] : bindings)
	{
		m_DescriptorSetLayouts.emplace_back();
		VkDescriptorSetLayout& descLayout = m_DescriptorSetLayouts.back();

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = (uint32_t)descLayoutBindings.size();
		createInfo.pBindings = descLayoutBindings.data();

		if (vkCreateDescriptorSetLayout(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &descLayout) != VK_SUCCESS)
			throw(false);
	}
}

void VulkanShader::addShaderStage(ShaderStage stage, const std::string& filepath)
{
	if (m_Stages.find(stage) != m_Stages.end())
		assert(false);

	m_Stages[stage] = std::make_shared<VulkanShaderStage>(stage, filepath);
	for (const ShaderInput& input : m_Stages[stage]->getInput())
		m_Input[input.Set].push_back(input);
}

std::shared_ptr<VulkanShaderStage> VulkanShader::getShaderStage(ShaderStage stage)
{
	if (m_Stages.find(stage) == m_Stages.end())
		assert(false);

	return m_Stages[stage];
}

std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> VulkanShader::getDescriptorSetLayoutBindings()
{
	std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings;

	for (auto& [set, input] : m_Input)
	{
		bindings[set] = std::vector<VkDescriptorSetLayoutBinding>();
		for (auto& i : input)
		{
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = i.Binding;
			layoutBinding.descriptorType = getInputType(i.Type);
			layoutBinding.descriptorCount = 1;
			layoutBinding.stageFlags = getStage(i.Stage);
			layoutBinding.pImmutableSamplers = nullptr;

			bindings[set].push_back(layoutBinding);
		}
	}

	return bindings;
}


VkPipelineShaderStageCreateInfo const VulkanShader::getStageCreateInfo(ShaderStage stage)
{
	if (m_Stages.find(stage) == m_Stages.end())
		assert(false);

	return m_Stages[stage]->getStageCreateInfo();
}

std::shared_ptr<VulkanShader>& ShaderManager::createShader(const std::string& name)
{
	if (m_Shaders.find(name) != m_Shaders.end())
		return m_Shaders[name];

	m_Shaders[name] = std::make_shared<VulkanShader>();
	return m_Shaders[name];
}

void ShaderManager::Clear()
{
	m_Shaders.clear();
}

std::shared_ptr<VulkanShader>& ShaderManager::getShader(const std::string& name)
{
	if (m_Shaders.find(name) == m_Shaders.end())
		assert(false);

	return m_Shaders[name];
}

shaderc_shader_kind getShadercKind(ShaderStage stage)
{
	switch (stage)
	{
	case ShaderStage::VERTEX:
		return shaderc_glsl_vertex_shader;
	case ShaderStage::FRAGMENT:
		return shaderc_glsl_fragment_shader;
	case ShaderStage::NONE:
		assert(false);
	}

	return shaderc_vertex_shader;
}

std::vector<uint32_t> VulkanShaderCompiler::compileVulkanShader(ShaderStage stage, const std::string& filepath, bool optimize)
{
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	std::string sourceCode = readFile(filepath);

	if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

	size_t begString = filepath.find_last_of('/');
	std::string shaderName = filepath.substr(begString + 1);

	shaderc::SpvCompilationResult module =
		compiler.CompileGlslToSpv(sourceCode, getShadercKind(stage), shaderName.c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << module.GetErrorMessage();
		return std::vector<uint32_t>();
	}

	return { module.cbegin(), module.cend() };
}

std::vector<ShaderInput> VulkanShaderCompiler::Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytecode)
{
	std::vector<ShaderInput> shaderInput;

	spirv_cross::Compiler compiler(shaderBytecode);
	spirv_cross::ShaderResources resources = compiler.get_shader_resources();

	// Uniform buffers
	for (const spirv_cross::Resource& uniformBuffer : resources.uniform_buffers)
	{
		ShaderInput uniformBufferInput;
		uniformBufferInput.Stage = stage;
		uniformBufferInput.DebugName = uniformBuffer.name;
		uniformBufferInput.Set = compiler.get_decoration(uniformBuffer.id, spv::DecorationDescriptorSet);
		uniformBufferInput.Binding = compiler.get_decoration(uniformBuffer.id, spv::DecorationBinding);

		if (uniformBuffer.name.find("Set") != std::string::npos)
			uniformBufferInput.Type = ShaderInputType::UNIFORM_BUFFER_SET;
		else 
			uniformBufferInput.Type = ShaderInputType::UNIFORM_BUFFER;

		shaderInput.push_back(uniformBufferInput);
	}

	// Samplers
	for (const spirv_cross::Resource& uniformBuffer : resources.sampled_images)
	{
		ShaderInput sampleImageInput;
		sampleImageInput.Stage = stage;
		sampleImageInput.DebugName = uniformBuffer.name;
		sampleImageInput.Set = compiler.get_decoration(uniformBuffer.id, spv::DecorationDescriptorSet);
		sampleImageInput.Binding = compiler.get_decoration(uniformBuffer.id, spv::DecorationBinding);
		sampleImageInput.Type = ShaderInputType::IMAGE_SAMPLER;

		shaderInput.push_back(sampleImageInput);
	}

	return shaderInput;
}
