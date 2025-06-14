#include "TerrainVirtualMap.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Core/Hash.h"

#include "DynamicVirtualTerrainDeserializer.h"

#include "stb_image/stb_image_write.h"

#include <iostream>
#include <cassert>
#include <fstream>
#include <execution>

#define INDIRECTION_COMPUTE_SHADER "Terrain/QuadTree/UpdateIndirectionTexture_comp.glsl"
#define STATUS_COMPUTE_SHADER "Terrain/QuadTree/UpdateStatusTexture_comp.glsl"

TerrainVirtualMap::TerrainVirtualMap(const VirtualTerrainMapSpecification& spec, const std::unique_ptr<TerrainData>& terrainData)
    : m_Specification(spec), m_TerrainData(terrainData)
{
    m_TerrainInfo = m_TerrainData->getSpecification().Info;
    int32_t availableSlots = m_Specification.PhysicalTextureSize / m_TerrainInfo.ChunkSize;

    {
        VulkanImageSpecification physicalTextureSpecification{};
        physicalTextureSpecification.Width = m_Specification.PhysicalTextureSize + availableSlots * 2; // Each slots takes 2 pixels padding
        physicalTextureSpecification.Height = m_Specification.PhysicalTextureSize + availableSlots * 2;
        physicalTextureSpecification.Format = VK_FORMAT_R16_UNORM;
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

    m_Deserializer = std::make_shared<DynamicVirtualTerrainDeserializer>(m_Specification, m_TerrainInfo.ChunkSize, m_TerrainData->getSpecification().ChunkedFilepath.Data);
}

uint32_t TerrainVirtualMap::pushLoadTasks(const glm::vec2& camPosition)
{
    glm::ivec2 intCameraPos = glm::ivec2(glm::max((int32_t)camPosition.x, 0), glm::max((int32_t)camPosition.y, 0));
    glm::ivec2 globalSnapPosition = glm::ivec2(intCameraPos.x / m_TerrainInfo.ChunkSize, intCameraPos.y / m_TerrainInfo.ChunkSize);

    if (m_LastCameraPosition != globalSnapPosition)
    {
        m_LastCameraPosition = globalSnapPosition;
        m_PositionsToProcess.push(globalSnapPosition);
    }

    if (!m_Deserializer->isAvailable() || m_PositionsToProcess.empty())
        return 0;

    m_IndirectionNodes.clear();
    m_StatusNodes.clear();

    globalSnapPosition = m_PositionsToProcess.front();
    m_PositionsToProcess.pop();

    uint32_t chunksRequested = 0;

    // Search for nodes that need to be loaded/unloaded
    m_NodesToUnload = m_ActiveNodesCPU;

    m_Updated = true;

    for (int32_t lod = m_TerrainInfo.LODCount - 1; lod >= 0; lod--)
    {
        int32_t ringLODSize = m_Specification.RingSizes[lod] / 2;
        if (ringLODSize <= 0)
            break;

        int32_t chunkSize = m_TerrainInfo.ChunkSize * (1 << lod);

        glm::ivec2 terrainCameraPosition = globalSnapPosition * m_TerrainInfo.ChunkSize;
        terrainCameraPosition.x = glm::max(terrainCameraPosition.x, chunkSize * ringLODSize);
        terrainCameraPosition.y = glm::max(terrainCameraPosition.y, chunkSize * ringLODSize);
        terrainCameraPosition.x = glm::min(terrainCameraPosition.x, m_TerrainInfo.TerrainSize - chunkSize * ringLODSize);
        terrainCameraPosition.y = glm::min(terrainCameraPosition.y, m_TerrainInfo.TerrainSize - chunkSize * ringLODSize);

        glm::ivec2 snapedPosition = terrainCameraPosition / chunkSize;

        int32_t minY = glm::max(snapedPosition.y - ringLODSize, 0);
        int32_t maxY = glm::min(snapedPosition.y + ringLODSize, m_TerrainInfo.TerrainSize / chunkSize);

        int32_t minX = glm::max(snapedPosition.x - ringLODSize, 0);
        int32_t maxX = glm::min(snapedPosition.x + ringLODSize, m_TerrainInfo.TerrainSize / chunkSize);

        for (uint32_t y = minY; y < maxY; y++)
            for (uint32_t x = minX; x < maxX; x++)
            {
                createLoadTask(TerrainChunk{ packOffset(x, y), uint32_t(lod) });
                chunksRequested++;
            }
    }

    // unload nodes
    for (size_t node : m_NodesToUnload)
    {
        m_ActiveNodesCPU.erase(node);
        int32_t slot = m_LastChunkSlot[node];
        m_AvailableSlots.insert(slot);

        const FileChunkProperties& props = m_TerrainData->getChunkProperty(node);

        m_StatusNodes.push_back(GPUStatusNode(props.Position, props.Mip, 0));
    }

    return chunksRequested;
}

void TerrainVirtualMap::createLoadTask(const TerrainChunk& chunk)
{
    size_t chunkHashValue = getChunkID(chunk.Offset, chunk.Lod);

    // Check for nodes to unload from disk
    if (m_NodesToUnload.find(chunkHashValue) != m_NodesToUnload.end())
        m_NodesToUnload.erase(chunkHashValue);

    // check for nodes that need to be loaded
    if (m_ActiveNodesCPU.find(chunkHashValue) == m_ActiveNodesCPU.end())
    {
        int32_t avSlot = INVALID_SLOT;
        m_ActiveNodesCPU.insert(chunkHashValue);

        if (m_LastChunkSlot.find(chunkHashValue) != m_LastChunkSlot.end())
            avSlot = m_LastChunkSlot.at(chunkHashValue);

        // check if the node may be already in the map but unloaded, if so, just remeber id
        if (avSlot != INVALID_SLOT && m_LastSlotChunk[avSlot] == chunkHashValue)
        {
            m_AvailableSlots.erase(avSlot);

            int32_t slotsPerRow = m_Specification.PhysicalTextureSize / m_TerrainInfo.ChunkSize;
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

            m_Deserializer->pushLoadTask(chunkHashValue, availableLoc, m_TerrainData->getChunkProperty(chunkHashValue));
        }
    }
}

uint32_t TerrainVirtualMap::updateMap(VkCommandBuffer cmdBuffer)
{
    m_Deserializer->Refresh(cmdBuffer, this);

    // number of loaded chunks
    return m_Deserializer->LastUpdate.IndirectionNodes.size();
}

void TerrainVirtualMap::blitNodes(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer, const std::vector<VkBufferImageCopy>& regions)
{
    // TODO: send buffer directly, no *
    m_PhysicalTexture->batchCopyBuffer(cmdBuffer, *StagingBuffer, regions);
}

void TerrainVirtualMap::updateIndirectionTexture(VkCommandBuffer cmdBuffer, uint32_t frameIndex)
{
    const std::vector<GPUIndirectionNode>& indirectionNodes = m_Deserializer->LastUpdate.IndirectionNodes;
    if (indirectionNodes.size() != 0)
    {
        for (GPUIndirectionNode node : indirectionNodes)
            m_IndirectionNodes.push_back(node);
    }

    if (m_IndirectionNodes.empty())
        return;

    m_IndirectionNodesStorage->getBuffer(frameIndex)->setDataCPU(m_IndirectionNodes.data(), m_IndirectionNodes.size() * sizeof(GPUIndirectionNode));

    int32_t size = (uint32_t)m_IndirectionNodes.size();
    int32_t invokes = (int32_t)glm::ceil(float(size) / 32.0f);

    {
        m_UpdateIndirectionComputePass.Prepare(cmdBuffer, frameIndex, sizeof(GPUVirtualMapProperties), &size);
        m_UpdateIndirectionComputePass.Dispatch(cmdBuffer, { invokes, 1, 1 });

        VulkanUtils::imageMemoryBarrier(cmdBuffer, m_IndirectionTexture, { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
    }

    m_IndirectionNodes.clear();
}

void TerrainVirtualMap::updateStatusTexture(VkCommandBuffer cmdBuffer, uint32_t frameIndex)
{
    const std::vector<GPUStatusNode>& statusNodes = m_Deserializer->LastUpdate.StatusNodes;
    if (statusNodes.size() != 0)
    {
        for (GPUStatusNode node : statusNodes)
        {
            uint32_t x = int(node.position & 0x0000ffff);
            uint32_t y = int(node.position & 0xffff0000) >> 16;

            uint32_t mip = node.mip & 0x0000ffff;

            if (m_ActiveNodesCPU.find(getChunkID(node.position, mip)) != m_ActiveNodesCPU.end())
                m_StatusNodes.push_back(node);
        }
    }

    if (m_StatusNodes.empty())
        return;

    m_StatusNodesStorage->getBuffer(frameIndex)->setDataCPU(m_StatusNodes.data(), m_StatusNodes.size() * sizeof(GPUStatusNode));

    int32_t size = (int32_t)m_StatusNodes.size();
    int32_t invokes = (int32_t)glm::ceil(float(size) / 32.0f);

    {
        m_UpdateStatusComputePass.Prepare(cmdBuffer, frameIndex, sizeof(GPUVirtualMapProperties), &size);
        m_UpdateStatusComputePass.Dispatch(cmdBuffer, { invokes, 1, 1 });

        VulkanUtils::imageMemoryBarrier(cmdBuffer, m_StatusTexture, { VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
    }

    m_StatusNodes.clear();
}

void TerrainVirtualMap::prepareForDeserialization(VkCommandBuffer cmdBuffer)
{
    VkImageSubresourceRange imgSubresource{};
    imgSubresource.aspectMask = m_PhysicalTexture->getSpecification().Aspect;
    imgSubresource.layerCount = 1;
    imgSubresource.levelCount = 1;
    imgSubresource.baseMipLevel = 0;
    VulkanUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void TerrainVirtualMap::prepareForRendering(VkCommandBuffer cmdBuffer)
{
    VkImageSubresourceRange imgSubresource{};
    imgSubresource.aspectMask = m_PhysicalTexture->getSpecification().Aspect;
    imgSubresource.layerCount = 1;
    imgSubresource.levelCount = 1;
    imgSubresource.baseMipLevel = 0;
    VulkanUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
}

bool TerrainVirtualMap::Updated()
{
    bool oldUpdated = m_Updated;
    m_Updated = false;
    return oldUpdated;
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
        uint32_t storeSize = m_TerrainInfo.TerrainSize / m_TerrainInfo.ChunkSize;
        storeSize = glm::max(storeSize, uint32_t(1 << MAX_LOD));

        VulkanImageSpecification indirectionTextureSpecification{};
        indirectionTextureSpecification.Width = storeSize;
        indirectionTextureSpecification.Height = storeSize;
        indirectionTextureSpecification.Mips = MAX_LOD;
        indirectionTextureSpecification.ImageViewOnMips = true;
        indirectionTextureSpecification.Format = VK_FORMAT_R32_UINT;
        indirectionTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        indirectionTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        indirectionTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        m_IndirectionTexture = std::make_shared<VulkanImage>(indirectionTextureSpecification);
        m_IndirectionTexture->Create();
    }

    // create shader
    {
        std::shared_ptr<VulkanShader>& indirectionCompute = ShaderManager::createShader(INDIRECTION_COMPUTE_SHADER);
        indirectionCompute->addShaderStage(ShaderStage::COMPUTE, INDIRECTION_COMPUTE_SHADER);
        indirectionCompute->createDescriptorSetLayouts();
    }

    // create pass
    {
        m_UpdateIndirectionComputePass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(INDIRECTION_COMPUTE_SHADER));

        for (uint32_t i = 0; i < MAX_LOD; i++)
            m_UpdateIndirectionComputePass.DescriptorSet->bindInput(0, 0, i, m_IndirectionTexture, (uint32_t)i);

        m_UpdateIndirectionComputePass.DescriptorSet->bindInput(1, 0, 0, m_IndirectionNodesStorage);
        m_UpdateIndirectionComputePass.DescriptorSet->Create();
        m_UpdateIndirectionComputePass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(INDIRECTION_COMPUTE_SHADER),
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
        uint32_t storeSize = m_TerrainInfo.TerrainSize / m_TerrainInfo.ChunkSize;
        storeSize = glm::max(storeSize, uint32_t(1 << MAX_LOD));

        VulkanImageSpecification statusTextureSpecification{};
        statusTextureSpecification.Width = storeSize;
        statusTextureSpecification.Height = storeSize;
        statusTextureSpecification.Mips = MAX_LOD;
        statusTextureSpecification.ImageViewOnMips = true;
        statusTextureSpecification.Format = VK_FORMAT_R8_UINT;
        statusTextureSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        statusTextureSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        statusTextureSpecification.MemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        m_StatusTexture = std::make_shared<VulkanImage>(statusTextureSpecification);
        m_StatusTexture->Create();
    }

    // create shader
    {
        std::shared_ptr<VulkanShader>& statusCompute = ShaderManager::createShader(STATUS_COMPUTE_SHADER);
        statusCompute->addShaderStage(ShaderStage::COMPUTE, STATUS_COMPUTE_SHADER);
        statusCompute->createDescriptorSetLayouts();
    }

    // create pass
    {
        m_UpdateStatusComputePass.DescriptorSet = std::make_shared<VulkanDescriptorSet>(ShaderManager::getShader(STATUS_COMPUTE_SHADER));

        for (uint32_t i = 0; i < MAX_LOD; i++)
            m_UpdateStatusComputePass.DescriptorSet->bindInput(0, 0, i, m_StatusTexture, (uint32_t)i);

        m_UpdateStatusComputePass.DescriptorSet->bindInput(1, 0, 0, m_StatusNodesStorage);
        m_UpdateStatusComputePass.DescriptorSet->Create();
        m_UpdateStatusComputePass.Pipeline = std::make_shared<VulkanComputePipeline>(ShaderManager::getShader(STATUS_COMPUTE_SHADER),
            uint32_t(sizeof(uint32_t) * 4));
    }
}
