#include "VulkanShader.h"

#include "VulkanDevice.h"
#include "VulkanPipeline.h"

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <filesystem>

#include <shaderc/shaderc.hpp>
#include <shaderc/glslc/file_includer.h>
#include <spirv_cross/spirv_glsl.hpp>

std::unordered_map<std::string, std::shared_ptr<VulkanShader>> ShaderManager::m_Shaders;

#define SOURCE_FILEPATH "Resources/Shaders/"
#define CACHE_FILEPATH "Cache/Shaders/"

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

static std::vector<uint32_t> readCachedShaderData(const std::string& filepath)
{
	FILE* f;
	fopen_s(&f, filepath.c_str(), "rb");

	if (!f)
	{
		std::cerr << "FIsierul " << filepath << " nu exista\n";
		assert(false);
	}

	fseek(f, 0, SEEK_END);
	uint64_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	std::vector<uint32_t> buffer(size / sizeof(uint32_t));
	fread(buffer.data(), sizeof(uint32_t), buffer.size(), f);

	fclose(f);

	return buffer;
}

static void writeShaderBinary(void* data, uint32_t size, const std::string& path)
{
	FILE* file;
	fopen_s(&file, path.c_str(), "wb");
	if (!file)
		assert(false);
	fwrite(data, sizeof(uint32_t), size, file);
	fclose(file);
}

VulkanShaderStage::VulkanShaderStage(ShaderStage stage, const std::string& filepath) : m_Stage(stage), m_Filepath(filepath)
{
	std::vector<uint32_t> data;

	size_t lastD = filepath.find_last_of('/');
	std::string directoryPath = std::string(filepath.begin(), filepath.begin() + (lastD != std::string::npos ? lastD : 0));
	std::string shaderName = std::string(filepath.begin(), filepath.begin() + filepath.find_last_of('.'));

	std::string codeFilepath = SOURCE_FILEPATH + filepath;
	std::string cacheFilepath = CACHE_FILEPATH + shaderName + ".spv";

	bool shouldRecompile = false;

	if (!std::filesystem::exists(cacheFilepath))
		shouldRecompile = true;
	else
	{
		std::filesystem::file_time_type lastModifiedCache = std::filesystem::last_write_time(cacheFilepath);
		std::filesystem::file_time_type lastModifiedShader = std::filesystem::last_write_time(codeFilepath);
		shouldRecompile = lastModifiedShader <= lastModifiedCache;
	}

	if (!std::filesystem::exists(CACHE_FILEPATH + directoryPath))
		std::filesystem::create_directories(CACHE_FILEPATH + directoryPath);

	if (std::filesystem::exists(cacheFilepath) && shouldRecompile)
		data = readCachedShaderData(cacheFilepath);

	else
	{
		data = VulkanShaderCompiler::compileVulkanShader(stage, codeFilepath);
		writeShaderBinary(data.data(), (uint32_t)data.size(), cacheFilepath);
	}

	m_Input = VulkanShaderCompiler::Reflect(stage, data);

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = data.size() * sizeof(uint32_t);
	createInfo.pCode = data.data();

	if (vkCreateShaderModule(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &m_ShaderModule) != VK_SUCCESS)
	{
		std::cerr << "Eroare shader\n";
		assert(false);
	}
}

VulkanShaderStage::~VulkanShaderStage()
{
	vkDestroyShaderModule(VulkanDevice::getVulkanDevice(), m_ShaderModule, nullptr);
}

void VulkanShaderStage::Recompile()
{
	std::vector<uint32_t> data = VulkanShaderCompiler::compileVulkanShader(m_Stage, SOURCE_FILEPATH + m_Filepath);

	std::string shaderName = std::string(m_Filepath.begin(), m_Filepath.begin() + m_Filepath.find_last_of('.'));
	std::string cacheFilepath = CACHE_FILEPATH + shaderName + ".spv";
	writeShaderBinary(data.data(), (uint32_t)data.size(), cacheFilepath);

	m_Input = VulkanShaderCompiler::Reflect(m_Stage, data);

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = data.size() * sizeof(uint32_t);
	createInfo.pCode = data.data();

	vkDestroyShaderModule(VulkanDevice::getVulkanDevice(), m_ShaderModule, nullptr);
	m_ShaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &m_ShaderModule) != VK_SUCCESS)
	{
		std::cerr << "Eroare shader\n";
		assert(false);
	}
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

	for (VkDescriptorSetLayout layout : m_DescriptorSetLayouts)
		vkDestroyDescriptorSetLayout(VulkanDevice::getVulkanDevice(), layout, nullptr);
	m_DescriptorSetLayouts.clear();

	for (auto& [set, descLayoutBindings] : bindings)
	{
		m_DescriptorSetLayouts.emplace_back();
		VkDescriptorSetLayout& descLayout = m_DescriptorSetLayouts.back();

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = (uint32_t)descLayoutBindings.size();
		createInfo.pBindings = descLayoutBindings.data();

		if (vkCreateDescriptorSetLayout(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &descLayout) != VK_SUCCESS)
			assert(false);
	}
}

void VulkanShader::addShaderStage(ShaderStage stage, const std::string& filepath)
{
	if (m_Stages.find(stage) != m_Stages.end())
		return;

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

bool VulkanShader::hasStage(ShaderStage stage)
{
	if (m_Stages.find(stage) == m_Stages.end())
		return false;
	return true;
}

std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> VulkanShader::getDescriptorSetLayoutBindings()
{
	std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings;

	for (auto& [set, input] : m_Input)
	{
		bindings[set] = std::vector<VkDescriptorSetLayoutBinding>();

		// double pass to match the same input on multiple stages
		std::unordered_map<std::string, VkDescriptorSetLayoutBinding> inputs;

		for (const ShaderInput& i : input)
		{
			if (inputs.find(i.DebugName) != inputs.end())
			{
				inputs[i.DebugName].stageFlags |= getStage(i.Stage);
				continue;
			}

			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = i.Binding;
			layoutBinding.descriptorType = getInputType(i.Type);
			layoutBinding.descriptorCount = i.Count;
			layoutBinding.stageFlags = getStage(i.Stage);
			layoutBinding.pImmutableSamplers = nullptr;
			inputs[i.DebugName] = layoutBinding;
		}

		for (auto& [name, layoutBinding] : inputs)
			bindings[set].push_back(layoutBinding);
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
	case ShaderStage::TESSELLATION_CONTROL:
		return shaderc_glsl_tess_control_shader;
	case ShaderStage::TESSELLATION_EVALUATION:
		return shaderc_glsl_tess_evaluation_shader;
	case ShaderStage::FRAGMENT:
		return shaderc_glsl_fragment_shader;
	case ShaderStage::COMPUTE:
		return shaderc_glsl_compute_shader;
	case ShaderStage::GEOMETRY:
		return shaderc_glsl_geometry_shader;
	case ShaderStage::NONE:
		assert(false);
	}

	return shaderc_vertex_shader;
}

static std::string readFile(const std::string& filepath)
{
	std::ifstream file(filepath);
	if (!file.good())
	{
		std::cerr << "FIsierul " << filepath << " nu exista\n";
		assert(false);
	}

	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string buffer(size, ' ');
	file.seekg(0);
	file.read(&buffer[0], size);

	return buffer;
}

std::vector<uint32_t> VulkanShaderCompiler::compileVulkanShader(ShaderStage stage, const std::string& filepath, bool optimize)
{
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	shaderc_util::FileFinder fileFinder;
	options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));

	std::string sourceCode = readFile(filepath);

	if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_performance); 
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
	options.SetGenerateDebugInfo();

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
		uniformBufferInput.Count = compiler.get_type(uniformBuffer.type_id).array[0] == 0 ? 1 : compiler.get_type(uniformBuffer.type_id).array[0];

		if (uniformBuffer.name.find("Set") != std::string::npos)
			uniformBufferInput.Type = ShaderInputType::UNIFORM_BUFFER_SET;
		else 
			uniformBufferInput.Type = ShaderInputType::UNIFORM_BUFFER;

		shaderInput.push_back(uniformBufferInput);
	}

	// Samplers
	for (const spirv_cross::Resource& sampler : resources.sampled_images)
	{
		ShaderInput sampleImageInput;
		sampleImageInput.Stage = stage;
		sampleImageInput.DebugName = sampler.name;
		sampleImageInput.Set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
		sampleImageInput.Binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
		sampleImageInput.Count = compiler.get_type(sampler.type_id).array[0] == 0 ? 1 : compiler.get_type(sampler.type_id).array[0];
		sampleImageInput.Type = ShaderInputType::COMBINED_IMAGE_SAMPLER;

		shaderInput.push_back(sampleImageInput);
	}

	for (const spirv_cross::Resource& texture : resources.separate_images)
	{
		ShaderInput sampleImageInput;
		sampleImageInput.Stage = stage;
		sampleImageInput.DebugName = texture.name;
		sampleImageInput.Set = compiler.get_decoration(texture.id, spv::DecorationDescriptorSet);
		sampleImageInput.Binding = compiler.get_decoration(texture.id, spv::DecorationBinding);
		sampleImageInput.Count = compiler.get_type(texture.type_id).array[0] == 0 ? 1 : compiler.get_type(texture.type_id).array[0];
		sampleImageInput.Type = ShaderInputType::TEXTURE;

		shaderInput.push_back(sampleImageInput);
	}

	for (const spirv_cross::Resource& sampler : resources.separate_samplers)
	{
		ShaderInput sampleImageInput;
		sampleImageInput.Stage = stage;
		sampleImageInput.DebugName = sampler.name;
		sampleImageInput.Set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
		sampleImageInput.Binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
		sampleImageInput.Count = compiler.get_type(sampler.type_id).array[0] == 0 ? 1 : compiler.get_type(sampler.type_id).array[0];
		sampleImageInput.Type = ShaderInputType::SAMPLER;

		shaderInput.push_back(sampleImageInput);
	}

	for (const spirv_cross::Resource& sampler : resources.storage_images)
	{
		ShaderInput sampleImageInput;
		sampleImageInput.Stage = stage;
		sampleImageInput.DebugName = sampler.name;
		sampleImageInput.Set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
		sampleImageInput.Binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
		sampleImageInput.Count = compiler.get_type(sampler.type_id).array[0] == 0 ? 1 : compiler.get_type(sampler.type_id).array[0];
		sampleImageInput.Type = ShaderInputType::STORAGE_IMAGE;

		shaderInput.push_back(sampleImageInput);
	}

	for (const spirv_cross::Resource& storageBuffer : resources.storage_buffers)
	{
		ShaderInput storageBufferInput;
		storageBufferInput.Stage = stage;
		storageBufferInput.DebugName = storageBuffer.name;
		storageBufferInput.Set = compiler.get_decoration(storageBuffer.id, spv::DecorationDescriptorSet);
		storageBufferInput.Binding = compiler.get_decoration(storageBuffer.id, spv::DecorationBinding);
		storageBufferInput.Count = compiler.get_type(storageBuffer.type_id).array[0] == 0 ? 1 : compiler.get_type(storageBuffer.type_id).array[0];

		size_t setWordPos = storageBuffer.name.rfind("Set");

		if (setWordPos != std::string::npos && setWordPos + 3 == storageBuffer.name.size())
			storageBufferInput.Type = ShaderInputType::STORAGE_BUFFER_SET;
		else
			storageBufferInput.Type = ShaderInputType::STORAGE_BUFFER;

		shaderInput.push_back(storageBufferInput);
	}

	return shaderInput;
}
