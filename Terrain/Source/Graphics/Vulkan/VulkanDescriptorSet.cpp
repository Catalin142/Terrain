#include "VulkanDescriptorSet.h"

#include "VulkanDevice.h"

#include <cassert>

// TODO: Modifica sa le ia pe baza la vsync, din renderer
#define FRAMES_IN_FLIGHT 2

VulkanDescriptorSet::VulkanDescriptorSet(const std::shared_ptr<VulkanShader>& shader) : m_Shader(shader) { }

VulkanDescriptorSet::~VulkanDescriptorSet()
{ 
	vkDestroyDescriptorPool(VulkanDevice::getVulkanDevice(), m_DescriptorPool, nullptr);
}

void VulkanDescriptorSet::Create()
{
	// ================== CREATE DESCRIPTOR POOL ==================
	VkDescriptorPoolSize poolSizes[6] = 
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 }
	};

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = 6;
	createInfo.pPoolSizes = poolSizes;
	createInfo.maxSets = 10;

	if (vkCreateDescriptorPool(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
		assert(false);


	// ================== CREATE DESCRIPTOR SETS ==================
	std::vector<VkDescriptorSetLayout> descriptorLayouts = m_Shader->getDescriptorSetLayouts();
	m_DescriptorSets.resize(m_Shader->getDescriptorSetCount());

	for (uint32_t frameIndex = 0; frameIndex < m_Shader->getDescriptorSetCount(); frameIndex++)
	{
		for (uint32_t currentSet = 0; currentSet < m_Shader->getNumberOfSets(); currentSet++)
		{
			std::vector<ShaderInput> inputs = m_Shader->getInputs(currentSet);
			VkDescriptorSetLayout layout = descriptorLayouts[currentSet];

			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VkDescriptorSet descSet;
			if (vkAllocateDescriptorSets(VulkanDevice::getVulkanDevice(), &allocInfo, &descSet) != VK_SUCCESS)
				assert(false);

			m_DescriptorSets[frameIndex].emplace_back(descSet);

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			for (const ShaderInput& input : inputs)
				for (uint32_t index = 0; index < input.Count; index++)
					writeDescriptorSets.emplace_back(getWriteDescriptorSet(input, index, descSet, frameIndex));
			
			vkUpdateDescriptorSets(VulkanDevice::getVulkanDevice(), (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
		}
	}
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanBuffer>& buffer)
{
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer->getBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = buffer->getSize();

	m_DescriptorBindings.BufferInfos[getHash(set, binding, index)] = bufferInfo;
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanBufferSet>& bufferSet)
{
	for (uint32_t i = 0; i < bufferSet->getCount(); i++)
	{
		VkDescriptorBufferInfo bufferInfo{};

		bufferInfo.buffer = bufferSet->getVkBuffer(i);
		bufferInfo.offset = 0;
		bufferInfo.range = bufferSet->getBufferSize();

		m_DescriptorBindings.BufferSetInfos[getHash(set, binding, index)].push_back(bufferInfo);
	}
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanImage>& image, uint32_t mip)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = image->getSpecification().UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT ? 
		VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = image->getVkImageView(mip);
	imageInfo.sampler = image->getVkSampler();

	m_DescriptorBindings.ImageInfos[getHash(set, binding, index)] = imageInfo;
}


void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, uint32_t index, const std::shared_ptr<VulkanSampler>& sampler)
{
	VkDescriptorImageInfo samplerInfo{};
	samplerInfo.sampler = sampler->Get();

	m_DescriptorBindings.ImageInfos[getHash(set, binding, index)] = samplerInfo;
}

std::vector<VkDescriptorSet> VulkanDescriptorSet::getDescriptorSet(uint32_t frameIndex)
{
	return m_DescriptorSets[frameIndex % m_Shader->getDescriptorSetCount()];
}

VkWriteDescriptorSet VulkanDescriptorSet::getWriteDescriptorSet(const ShaderInput& input, uint32_t index, VkDescriptorSet set, uint32_t frame = 0)
{
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = set;
	descriptorWrite.dstBinding = input.Binding;
	descriptorWrite.dstArrayElement = index;
	descriptorWrite.descriptorCount = 1;

	switch (input.Type)
	{
	case ShaderInputType::UNIFORM_BUFFER:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.pBufferInfo = &m_DescriptorBindings.BufferInfos[getHash(input.Set, input.Binding, index)];
		break;

	case ShaderInputType::STORAGE_BUFFER:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite.pBufferInfo = &m_DescriptorBindings.BufferInfos[getHash(input.Set, input.Binding, index)];
		break;

	case ShaderInputType::UNIFORM_BUFFER_SET:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.pBufferInfo = &m_DescriptorBindings.BufferSetInfos[getHash(input.Set, input.Binding, index)][frame];
		break;

	case ShaderInputType::STORAGE_BUFFER_SET:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite.pBufferInfo = &m_DescriptorBindings.BufferSetInfos[getHash(input.Set, input.Binding, index)][frame];
		break;

	case ShaderInputType::COMBINED_IMAGE_SAMPLER:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.pImageInfo = &m_DescriptorBindings.ImageInfos[getHash(input.Set, input.Binding, index)];
		break;

	case ShaderInputType::TEXTURE:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		descriptorWrite.pImageInfo = &m_DescriptorBindings.ImageInfos[getHash(input.Set, input.Binding, index)];
		break;

	case ShaderInputType::SAMPLER:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptorWrite.pImageInfo = &m_DescriptorBindings.ImageInfos[getHash(input.Set, input.Binding, index)];
		break;

	case ShaderInputType::STORAGE_IMAGE:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite.pImageInfo = &m_DescriptorBindings.ImageInfos[getHash(input.Set, input.Binding, index)];
		break;

	default:
		assert(false);
	}

	return descriptorWrite;
}

uint32_t VulkanDescriptorSet::getHash(uint32_t set, uint32_t binding, uint32_t index)
{
	return (index * MAXIMUM_NUMBER_OF_SETS_PER_STAGE * MAXIMUM_ARRAY_ELEMENTS) + set * MAXIMUM_NUMBER_OF_SETS_PER_STAGE + binding;
}
