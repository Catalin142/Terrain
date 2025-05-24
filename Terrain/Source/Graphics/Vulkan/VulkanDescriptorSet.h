#pragma once

#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanImage.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>

struct DescriptorBindings
{
	std::unordered_map<uint32_t, VkDescriptorBufferInfo> BufferInfos;
	std::unordered_map<uint32_t, std::vector<VkDescriptorBufferInfo>> BufferSetInfos;
	std::unordered_map<uint32_t, VkDescriptorImageInfo> ImageInfos;
};

class VulkanDescriptorSet
{
public:
	VulkanDescriptorSet(const std::shared_ptr<VulkanShader>& shader);
	~VulkanDescriptorSet();

	void Create();

	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanBuffer>& buffer);
	void bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanBufferSet>& bufferSet);
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
