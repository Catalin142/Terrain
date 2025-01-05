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
#include <Core/Instrumentor.h>

TerrainVirtualMap::TerrainVirtualMap(const VirtualTerrainMapSpecification& spec) : m_Specification(spec)
{
    int32_t availableSlots = m_Specification.PhysicalTextureSize / m_Specification.ChunkSize;

    {
        VulkanImageSpecification physicalTextureSpecification{};
        physicalTextureSpecification.Width = m_Specification.PhysicalTextureSize + availableSlots * 2; // Each slots takes 2 pixels padding
        physicalTextureSpecification.Height = m_Specification.PhysicalTextureSize + availableSlots * 2;
        physicalTextureSpecification.Format = m_Specification.Format;
        physicalTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        physicalTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        physicalTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        m_PhysicalTexture = std::make_shared<VulkanImage>(physicalTextureSpecification);
        m_PhysicalTexture->Create();
    }

    availableSlots *= availableSlots;
    for (int32_t slot = 0; slot < availableSlots; slot++)
        m_AvailableSlots.insert(slot);
    m_LastSlotChunk.resize(availableSlots);

    m_IndirectionNodes.reserve(1024);
    createIndirectionResources();

    m_StatusNodes.reserve(1024);
    createStatusResources();

    m_Deserializer = std::make_shared<DynamicVirtualTerrainDeserializer>(m_Specification);
}

void TerrainVirtualMap::pushLoadTasks(const std::vector<TerrainChunk>& chunks)
{
    Instrumentor::Get().beginTimer("VirtualMap Update");

    // Search for nodes that need to be loaded/unloaded
    m_NodesToUnload = m_ActiveNodes;

    m_IndirectionNodes.clear();
    m_StatusNodes.clear();

    // Check for nodes to load / unload
    for (const TerrainChunk& chunk : chunks)
    {
        size_t chunkHashValue = getChunkID(chunk.Offset, chunk.Lod);

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
            {
                m_AvailableSlots.erase(avSlot);

                int32_t slotsPerRow = m_Specification.PhysicalTextureSize / m_Specification.ChunkSize;
                int32_t x = avSlot / slotsPerRow;
                int32_t y = avSlot % slotsPerRow;

                m_IndirectionNodes.push_back(GPUIndirectionNode(chunk.Offset, packOffset(x, y), chunk.Lod));
                m_StatusNodes.push_back(GPUStatusNode(chunk.Offset, chunk.Lod, 1));
            }
            else
            {
                assert(m_AvailableSlots.size() != 0);

                int32_t availableLoc = *m_AvailableSlots.begin();
                m_AvailableSlots.erase(availableLoc);

                // Cache
                m_LastChunkSlot[chunkHashValue] = availableLoc;
                m_LastSlotChunk[availableLoc] = chunkHashValue;

                m_Deserializer->pushLoadTask(chunkHashValue, availableLoc, m_ChunkProperties[chunkHashValue], VirtualTextureType::HEIGHT);
            }
        }
    }

    // unload nodes
    for (size_t node : m_NodesToUnload)
    {
        if (m_LastChunkSlot.count(node) == 0)
            continue;

        m_ActiveNodes.erase(node);
        int32_t slot = m_LastChunkSlot[node];
        m_AvailableSlots.insert(slot);

        const VirtualTerrainChunkProperties& props = m_ChunkProperties[node];
        
        m_StatusNodes.push_back(GPUStatusNode(props.Position, props.Mip, 0));
    }

    Instrumentor::Get().endTimer("VirtualMap Update");
}

void TerrainVirtualMap::updateVirtualMap(VkCommandBuffer cmdBuffer)
{
    m_Deserializer->Refresh(cmdBuffer, this);
}

void TerrainVirtualMap::blitNodes(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer, const std::vector<VkBufferImageCopy>& regions)
{
    // TODO: send buffer directly, no *
    m_PhysicalTexture->batchCopyBuffer(cmdBuffer, *StagingBuffer, regions);
}

void TerrainVirtualMap::addVirtualChunkProperty(size_t chunk, const VirtualTerrainChunkProperties& props)
{
    m_ChunkProperties[chunk] = props;
}

void TerrainVirtualMap::updateResources(VkCommandBuffer cmdBuffer)
{
    updateIndirectionTexture(cmdBuffer);
    updateStatusTexture(cmdBuffer);
}

void TerrainVirtualMap::updateIndirectionTexture(VkCommandBuffer cmdBuffer)
{
    const std::vector<GPUIndirectionNode>& indirectionNodes = m_Deserializer->LastUpdate.IndirectionNodes;
    if (indirectionNodes.size() != 0)
        std::copy(indirectionNodes.begin(), indirectionNodes.end(), std::back_inserter(m_IndirectionNodes));

    if (m_IndirectionNodes.empty())
        return;

    m_IndirectionNodesStorage->getCurrentFrameBuffer()->setDataCPU(m_IndirectionNodes.data(), m_IndirectionNodes.size() * sizeof(GPUIndirectionNode));

    uint32_t size = (uint32_t)m_IndirectionNodes.size();
    int32_t invokes = (int32_t)glm::ceil(float(size) / 32.0f);

    {
        VulkanRenderer::dispatchCompute(cmdBuffer, m_UpdateIndirectionComputePass, { invokes, 1, 1 },
            sizeof(GPUVirtualMapProperties), &size);

        VulkanComputePipeline::imageMemoryBarrier(cmdBuffer, m_IndirectionTexture, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 6);
    }
}

void TerrainVirtualMap::updateStatusTexture(VkCommandBuffer cmdBuffer)
{
    const std::vector<GPUStatusNode>& statusNodes = m_Deserializer->LastUpdate.StatusNodes;
    if (statusNodes.size() != 0)
        std::copy(statusNodes.begin(), statusNodes.end(), std::back_inserter(m_StatusNodes));

    if (m_StatusNodes.empty())
        return;

    m_StatusNodesStorage->getCurrentFrameBuffer()->setDataCPU(m_StatusNodes.data(), m_StatusNodes.size() * sizeof(GPUStatusNode));

    uint32_t size = (uint32_t)m_StatusNodes.size();
    int32_t invokes = (int32_t)glm::ceil(float(size) / 32.0f);

    {
        VulkanRenderer::dispatchCompute(cmdBuffer, m_UpdateStatusComputePass, { invokes, 1, 1 },
            sizeof(GPUVirtualMapProperties), &size);

        VulkanComputePipeline::imageMemoryBarrier(cmdBuffer, m_StatusTexture, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 6);
    }
}

void TerrainVirtualMap::getChunksToLoad(const glm::vec2& camPosition, std::vector<TerrainChunk>& chunks)
{
    // find a way to optimizie this, multithread?
    for (uint32_t lod = 0; lod < MAX_LOD; lod++)
    {
        int32_t ringLODSize = m_Specification.RingSizes[lod];
        if (ringLODSize <= 0)
            break;
        
        int32_t chunkSize = m_Specification.ChunkSize * (1 << lod);

        glm::ivec2 snapedPosition;
        snapedPosition.x = int32_t(camPosition.x) / chunkSize;
        snapedPosition.y = int32_t(camPosition.y) / chunkSize;

        int32_t minY = glm::max(int32_t(snapedPosition.y) - ringLODSize / 2, 0);
        int32_t maxY = glm::min(int32_t(snapedPosition.y) + ringLODSize / 2, (int32_t)m_Specification.VirtualTextureSize / chunkSize);

        int32_t minX = glm::max(int32_t(snapedPosition.x) - ringLODSize / 2, 0);
        int32_t maxX = glm::min(int32_t(snapedPosition.x) + ringLODSize / 2, (int32_t)m_Specification.VirtualTextureSize / chunkSize);

        for (int32_t y = minY; y < maxY; y++)
            for (int32_t x = minX; x < maxX; x++)
                chunks.push_back(TerrainChunk{ packOffset(x, y), lod });
    }
}

void TerrainVirtualMap::prepareForDeserialization(VkCommandBuffer cmdBuffer)
{
    VkImageSubresourceRange imgSubresource{};
    imgSubresource.aspectMask = m_PhysicalTexture->getSpecification().Aspect;
    imgSubresource.layerCount = 1;
    imgSubresource.levelCount = 1;
    imgSubresource.baseMipLevel = 0;
    VkUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void TerrainVirtualMap::prepareForRendering(VkCommandBuffer cmdBuffer)
{
    VkImageSubresourceRange imgSubresource{};
    imgSubresource.aspectMask = m_PhysicalTexture->getSpecification().Aspect;
    imgSubresource.layerCount = 1;
    imgSubresource.levelCount = 1;
    imgSubresource.baseMipLevel = 0;
    VkUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
}


void TerrainVirtualMap::createIndirectionResources()
{
    {
        VulkanBufferProperties indirectionBufferProperties;
        indirectionBufferProperties.Size = 1024 * (uint32_t)sizeof(GPUIndirectionNode);
        indirectionBufferProperties.Type = BufferType::STORAGE_BUFFER;
        indirectionBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

        m_IndirectionNodesStorage = std::make_shared<VulkanBufferSet>(2, indirectionBufferProperties);
    }

    {
        uint32_t storeSize = m_Specification.VirtualTextureSize / m_Specification.ChunkSize;
        storeSize = glm::max(storeSize, uint32_t(1 << MAX_LOD));

        VulkanImageSpecification indirectionTextureSpecification{};
        indirectionTextureSpecification.Width = storeSize;
        indirectionTextureSpecification.Height = storeSize;
        indirectionTextureSpecification.Mips = MAX_LOD;
        indirectionTextureSpecification.ImageViewOnMips = true;
        indirectionTextureSpecification.Format = VK_FORMAT_R32_UINT;
        indirectionTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        indirectionTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        indirectionTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        m_IndirectionTexture = std::make_shared<VulkanImage>(indirectionTextureSpecification);
        m_IndirectionTexture->Create();
    }

    // create shader
    {
        std::shared_ptr<VulkanShader>& indirectionCompute = ShaderManager::createShader("_UpdateIndirectionCompute");
        indirectionCompute->addShaderStage(ShaderStage::COMPUTE, "VirtualTexture/UpdateIndirectionTexture_comp.glsl");
        indirectionCompute->createDescriptorSetLayouts();
    }

    // create pass
    {
        m_UpdateIndirectionComputePass = std::make_shared<VulkanComputePass>();
        m_UpdateIndirectionComputePass->DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_UpdateIndirectionCompute"));

        for (uint32_t i = 0; i < MAX_LOD; i++)
            m_UpdateIndirectionComputePass->DescriptorSet->bindInput(0, 0, i, m_IndirectionTexture, (uint32_t)i);

        m_UpdateIndirectionComputePass->DescriptorSet->bindInput(1, 0, 0, m_IndirectionNodesStorage);
        m_UpdateIndirectionComputePass->DescriptorSet->Create();
        m_UpdateIndirectionComputePass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_UpdateIndirectionCompute"),
            uint32_t(sizeof(uint32_t) * 4));
    }
}

void TerrainVirtualMap::createStatusResources()
{
    {
        VulkanBufferProperties statusBufferProperties;
        statusBufferProperties.Size = 1024 * (uint32_t)sizeof(GPUStatusNode);
        statusBufferProperties.Type = BufferType::STORAGE_BUFFER;
        statusBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

        m_StatusNodesStorage = std::make_shared<VulkanBufferSet>(2, statusBufferProperties);
    }

    {
        uint32_t storeSize = m_Specification.VirtualTextureSize / m_Specification.ChunkSize;
        storeSize = glm::max(storeSize, uint32_t(1 << MAX_LOD));

        VulkanImageSpecification statusTextureSpecification{};
        statusTextureSpecification.Width = storeSize;
        statusTextureSpecification.Height = storeSize;
        statusTextureSpecification.Mips = MAX_LOD;
        statusTextureSpecification.ImageViewOnMips = true;
        statusTextureSpecification.Format = VK_FORMAT_R8_UINT;
        statusTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        statusTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        statusTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        m_StatusTexture = std::make_shared<VulkanImage>(statusTextureSpecification);
        m_StatusTexture->Create();
    }

    // create shader
    {
        std::shared_ptr<VulkanShader>& statusCompute = ShaderManager::createShader("_UpdateStatusCompute");
        statusCompute->addShaderStage(ShaderStage::COMPUTE, "VirtualTexture/UpdateStatusTexture_comp.glsl");
        statusCompute->createDescriptorSetLayouts();
    }

    // create pass
    {
        m_UpdateStatusComputePass = std::make_shared<VulkanComputePass>();
        m_UpdateStatusComputePass->DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader("_UpdateStatusCompute"));

        for (uint32_t i = 0; i < MAX_LOD; i++)
            m_UpdateStatusComputePass->DescriptorSet->bindInput(0, 0, i, m_StatusTexture, (uint32_t)i);

        m_UpdateStatusComputePass->DescriptorSet->bindInput(1, 0, 0, m_StatusNodesStorage);
        m_UpdateStatusComputePass->DescriptorSet->Create();
        m_UpdateStatusComputePass->Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader("_UpdateStatusCompute"),
            uint32_t(sizeof(uint32_t) * 4));
    }
}
