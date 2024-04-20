#pragma once

#include "VulkanUniformBuffer.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>

struct DescriptorBindings
{
	std::unordered_map<uint32_t, VkDescriptorBufferInfo> UniformBufferInfos;
	std::unordered_map<uint32_t, std::vector<VkDescriptorBufferInfo>> UniformBufferSetInfos;
	std::unordered_map<uint32_t, VkDescriptorImageInfo> ImageInfos;
};

class VulkanDescriptorSet
{
public:
	VulkanDescriptorSet(const std::shared_ptr<VulkanShader>& shader);
	~VulkanDescriptorSet();

	void Create();

	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanUniformBuffer>& buffer);
	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanUniformBufferSet>& bufferSet);
	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanTexture>& texture);
	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanImage>& image, uint32_t mip = 0);
	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanSampler>& sampler);

	std::vector<VkDescriptorSet> getDescriptorSet(uint32_t frameIndex);
	uint32_t getNumberOfSets() { return m_Shader->getNumberOfSets(); }
	
	VkWriteDescriptorSet getWriteDescriptorSet(const ShaderInput& input, uint32_t index, VkDescriptorSet set, uint32_t frame);

private:
	uint32_t getHash(uint32_t set, uint32_t binding, uint32_t index);

private:
	std::shared_ptr<VulkanShader> m_Shader;

	// Per frame
	std::vector<std::vector<VkDescriptorSet>> m_DescriptorSets;
	VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

	DescriptorBindings m_DescriptorBindings;
};
