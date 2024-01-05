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
	VkDescriptorPoolSize poolSizes[2] = 
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
	};

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = 2;
	createInfo.pPoolSizes = poolSizes;
	createInfo.maxSets = 10;

	if (vkCreateDescriptorPool(VulkanDevice::getVulkanDevice(), &createInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
		throw(false);


	// ================== CREATE DESCRIPTOR SETS ==================
	std::vector<VkDescriptorSetLayout> descriptorLayouts = m_Shader->getDescriptorSetLayouts();
	m_DescriptorSets.resize(FRAMES_IN_FLIGHT);

	for (uint32_t frameIndex = 0; frameIndex < FRAMES_IN_FLIGHT; frameIndex++)
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
				throw(false);

			m_DescriptorSets[frameIndex].emplace_back(descSet);

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			for (const ShaderInput& input : inputs)
				writeDescriptorSets.emplace_back(getWriteDescriptorSet(input, descSet, frameIndex));
			
			vkUpdateDescriptorSets(VulkanDevice::getVulkanDevice(), (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
		}
	}
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, const std::shared_ptr<VulkanUniformBuffer>& buffer)
{
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer->getBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = buffer->getSize();

	m_DescriptorBindings.UniformBufferInfos[getHash(set, binding)] = bufferInfo;
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, const std::shared_ptr<VulkanUniformBufferSet>& bufferSet)
{
	for (uint32_t frame = 0; frame < bufferSet->getFrameCount(); frame++)
	{
		VkDescriptorBufferInfo bufferInfo{};

		bufferInfo.buffer = bufferSet->getBuffer(frame);
		bufferInfo.offset = 0;
		bufferInfo.range = bufferSet->getBufferSize();

		m_DescriptorBindings.UniformBufferSetInfos[getHash(set, binding)].push_back(bufferInfo);
	}
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, const std::shared_ptr<VulkanTexture>& texture)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture->getVkImageView();
	imageInfo.sampler = texture->getVkSampler();

	m_DescriptorBindings.ImageInfos[getHash(set, binding)] = imageInfo;
}

void VulkanDescriptorSet::bindInput(uint32_t set, uint32_t binding, const std::shared_ptr<VulkanImage>& image)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = image->getVkImageView();
	imageInfo.sampler = image->getVkSampler();

	m_DescriptorBindings.ImageInfos[getHash(set, binding)] = imageInfo;
}

std::vector<VkDescriptorSet> VulkanDescriptorSet::getDescriptorSet(uint32_t frameIndex)
{
	assert(frameIndex < m_DescriptorSets.size());
	return m_DescriptorSets[frameIndex];
}

VkWriteDescriptorSet VulkanDescriptorSet::getWriteDescriptorSet(const ShaderInput& input, VkDescriptorSet set, uint32_t frame = 0)
{
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = set;
	descriptorWrite.dstBinding = input.Binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorCount = 1;

	switch (input.Type)
	{
	case ShaderInputType::UNIFORM_BUFFER:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.pBufferInfo = &m_DescriptorBindings.UniformBufferInfos[getHash(input.Set, input.Binding)];
		break;

	case ShaderInputType::UNIFORM_BUFFER_SET:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.pBufferInfo = &m_DescriptorBindings.UniformBufferSetInfos[getHash(input.Set, input.Binding)][frame];
		break;

	case ShaderInputType::IMAGE_SAMPLER:
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.pImageInfo = &m_DescriptorBindings.ImageInfos[getHash(input.Set, input.Binding)];
		break;

	default:
		assert(false);
	}

	return descriptorWrite;


}

uint32_t VulkanDescriptorSet::getHash(uint32_t set, uint32_t binding)
{
	return set * MAXIMUM_NUMBER_OF_SETS_PER_STAGE + binding;
}
