#pragma once

#include "VulkanShaderInput.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <thread>

#include <vulkan/vulkan.h>

namespace VulkanShaderCompiler
{
	static std::vector<uint32_t> compileVulkanShader(ShaderStage stage, const std::string& filepath, bool optimize = true);
	static std::vector<ShaderInput> Reflect(ShaderStage stage, const std::vector<uint32_t>& shaderBytecode);
}

class VulkanShaderStage
{
public:
	VulkanShaderStage(ShaderStage stage, const std::string& filepath);
	~VulkanShaderStage();

	void Recompile();

	inline VkShaderModule getHandle() const { return m_ShaderModule; };
	const std::vector<ShaderInput>& getInput() { return m_Input; }

	VkPipelineShaderStageCreateInfo const getStageCreateInfo();

private:
	ShaderStage m_Stage;
	std::string m_Filepath;

	VkShaderModule m_ShaderModule;
	
	std::vector<ShaderInput> m_Input;
};

class VulkanShader
{
public:
	VulkanShader() = default;
	~VulkanShader();

	void createDescriptorSetLayouts();

	void addShaderStage(ShaderStage stage, const std::string& filepath);
	std::shared_ptr<VulkanShaderStage> getShaderStage(ShaderStage stage);
	bool hasStage(ShaderStage stage);

	std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> getDescriptorSetLayoutBindings();
	const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts()
	{
		return m_DescriptorSetLayouts;
	}
	VkDescriptorSetLayout getDescriptorSetLayout(uint32_t set) { return m_DescriptorSetLayouts[set]; }

	VkPipelineShaderStageCreateInfo const getStageCreateInfo(ShaderStage stage);

	const std::vector<ShaderInput> getInputs(uint32_t set) { return m_Input[set]; }
	uint32_t getNumberOfSets() { return (uint32_t)m_DescriptorSetLayouts.size(); }

private:
	std::unordered_map<ShaderStage, std::shared_ptr<VulkanShaderStage>> m_Stages;
	std::map<uint32_t, std::vector<ShaderInput>> m_Input;

	std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
};

class ShaderManager
{
public:
	static std::shared_ptr<VulkanShader>& createShader(const std::string& name);
	static std::shared_ptr<VulkanShader>& getShader(const std::string& name);

	static void Clear();

private:
	ShaderManager() = default;
	~ShaderManager() = default;
	
private:
	static std::unordered_map<std::string, std::shared_ptr<VulkanShader>> m_Shaders;
};