#include "TerrainVirtualMap.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Core/Hash.h"

#include "DynamicVirtualTerrainDeserializer.h"
#include "VMUtils.h"

#include "stb_image/stb_image_write.h"

#include <iostream>
#include <cassert>
#include <fstream>
#include <execution>

TerrainVirtualMap::TerrainVirtualMap(const VirtualTerrainMapSpecification& spec) : m_Specification(spec)
{
    m_Specification.LODCount = glm::clamp(m_Specification.LODCount, 0u, MAX_LOD);

    {
        VulkanImageSpecification physicalTextureSpecification{};
        physicalTextureSpecification.Width = m_Specification.PhysicalTextureSize;
        physicalTextureSpecification.Height = m_Specification.PhysicalTextureSize;
        physicalTextureSpecification.Format = m_Specification.Format;
        physicalTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        physicalTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        physicalTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        m_PhysicalTexture = std::make_shared<VulkanImage>(physicalTextureSpecification);
        m_PhysicalTexture->Create();
    }

    // To update indirection texture, we can send to the gpu all the nodes that were updated that frame and update the indirection tex
    // using a compute shader :-?
    {
        VulkanImageSpecification indirectionTextureSpecification{};
        indirectionTextureSpecification.Width = m_Specification.IndirectionTextureSize;
        indirectionTextureSpecification.Height = m_Specification.IndirectionTextureSize;
        indirectionTextureSpecification.Mips = MAX_LOD;
        indirectionTextureSpecification.ImageViewOnMips = true;
        indirectionTextureSpecification.Format = VK_FORMAT_R32_UINT;
        indirectionTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        indirectionTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        indirectionTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        m_IndirectionTexture = std::make_shared<VulkanImage>(indirectionTextureSpecification);
        m_IndirectionTexture->Create();
    }

    int32_t availableSlots = m_Specification.PhysicalTextureSize / m_Specification.ChunkSize;
    availableSlots *= availableSlots;
    for (int32_t slot = 0; slot < availableSlots; slot++)
        m_AvailableSlots.insert(slot);
    m_LastSlotChunk.resize(availableSlots);

    createCompute();
}

void TerrainVirtualMap::updateVirtualMap(const std::vector<TerrainChunk>& chunks)
{
    // Search for nodes that need to be loaded/unloaded

    m_NodesToUnload = m_ActiveNodes;

    // Check for nodes to load / unload
    for (const TerrainChunk& chunk : chunks)
    {
        int32_t mip = log2(chunk.Lod);

        size_t chunkHashValue = getChunkID(((uint32_t)chunk.Offset.x / m_Specification.ChunkSize) >> mip,
            ((uint32_t)chunk.Offset.y / m_Specification.ChunkSize) >> mip, mip);

        if (m_ChunkProperties.find(chunkHashValue) == m_ChunkProperties.end())
            assert(false);

        // Check for nodes to unload from disk
        if (m_NodesToUnload.find(chunkHashValue) != m_NodesToUnload.end())
            m_NodesToUnload.erase(chunkHashValue);

        // check for nodes that need to be loaded
        if (m_ActiveNodes.find(chunkHashValue) == m_ActiveNodes.end())
        {
            m_ActiveNodes.insert(chunkHashValue);
            int32_t avSlot = -1;

            if (m_LastChunkSlot.find(chunkHashValue) != m_LastChunkSlot.end())
                avSlot = m_LastChunkSlot.at(chunkHashValue);

            // check if the node may be already in the map but unloaded, if so, just remeber id
            if (avSlot != -1 && m_LastSlotChunk.at(avSlot) == chunkHashValue)
                m_AvailableSlots.erase(avSlot);
            else
            {
                assert(m_AvailableSlots.size() != 0);

                int32_t availableLoc = *m_AvailableSlots.begin();
                m_AvailableSlots.erase(availableLoc);

                // Cache
                m_LastChunkSlot[chunkHashValue] = availableLoc;
                m_LastSlotChunk[availableLoc] = chunkHashValue;

                DynamicVirtualTerrainDeserializer::Get()->pushLoadTask(chunkHashValue, packOffset(chunk.Offset.x, chunk.Offset.y),
                    mip, VirtualTextureType::HEIGHT);
            }
        }
    }

    refreshNodes();
}

void TerrainVirtualMap::refreshNodes()
{
    // unload nodes
    for (size_t node : m_NodesToUnload)
    {
        if (m_LastChunkSlot.count(node) == 0)
            continue;

        m_ActiveNodes.erase(node);
        int32_t slot = m_LastChunkSlot[node];
        m_AvailableSlots.insert(slot);
    }
}

uint32_t TerrainVirtualMap::blitNode(size_t chunk, VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer)
{
    VkImageSubresourceRange imgSubresource{};
    imgSubresource.aspectMask = m_PhysicalTexture->getSpecification().Aspect;
    imgSubresource.layerCount = 1;
    imgSubresource.levelCount = 1;
    imgSubresource.baseMipLevel = 0;

    int32_t iChunkSize = (int32_t)m_Specification.ChunkSize;

    int32_t availableLoc = m_LastChunkSlot[chunk];
    
    int32_t slotsPerRow = m_Specification.PhysicalTextureSize / m_Specification.ChunkSize;
    int32_t x = availableLoc / slotsPerRow;
    int32_t y = availableLoc % slotsPerRow;

    VkUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_PhysicalTexture->copyBuffer(cmdBuffer, *StagingBuffer->getBaseBuffer(), 0, glm::uvec2(m_Specification.ChunkSize, m_Specification.ChunkSize), 
        glm::uvec2(x * iChunkSize, y * iChunkSize));
    VkUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    return packOffset(x, y);
}

void TerrainVirtualMap::addChunkFileOffset(size_t chunk, uint32_t prop)
{
    m_ChunkProperties[chunk] = prop;
}

uint32_t TerrainVirtualMap::getChunkFileOffset(size_t chunk)
{
    assert(m_ChunkProperties.find(chunk) != m_ChunkProperties.end());
    return m_ChunkProperties[chunk];
}

void TerrainVirtualMap::createCompute()
{
    // create resources
    {
        m_LoadedNodesUB = std::make_shared<VulkanUniformBuffer>(64 * (uint32_t)sizeof(LoadedNode));
    }

    // create shader
    {
        std::shared_ptr<VulkanShader>& indirectionCompute = ShaderManager::createShader("_IndirectionCompute");
        indirectionCompute->addShaderStage(ShaderStage::COMPUTE, "VirtualTexture/IndirectionTexture_comp.glsl");
        indirectionCompute->createDescriptorSetLayouts();
    }

    // create pass
    {
        m_IndirectionTextureUpdatePass = std::make_shared<VulkanComputePass>();
        m_IndirectionTextureUpdatePass->DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_IndirectionCompute"));

        for (uint32_t i = 0; i < MAX_LOD; i++)
        {
            m_IndirectionTextureUpdatePass->DescriptorSet->bindInput(0, 0, i, m_IndirectionTexture, (uint32_t)i);
        }

        m_IndirectionTextureUpdatePass->DescriptorSet->bindInput(1, 0, 0, m_LoadedNodesUB);
        m_IndirectionTextureUpdatePass->DescriptorSet->Create();
        m_IndirectionTextureUpdatePass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_IndirectionCompute"),
            uint32_t(sizeof(VirtualMapProperties)));
    }
}

void TerrainVirtualMap::updateIndirectionTexture(VkCommandBuffer cmdBuffer, std::vector<LoadedNode>& nodes)
{
    m_LoadedNodesUB->setData(nodes.data(), nodes.size() * sizeof(LoadedNode));

    m_VMProps.chunkSize = m_Specification.ChunkSize;
    m_VMProps.loadedNodesCount = nodes.size();

    {
        VulkanRenderer::dispatchCompute(cmdBuffer, m_IndirectionTextureUpdatePass, { 1, 1, 1 },
            sizeof(VirtualMapProperties), &m_VMProps);
    }
}