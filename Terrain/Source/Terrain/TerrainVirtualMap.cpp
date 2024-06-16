#include "TerrainVirtualMap.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Core/Hash.h"

#include "stb_image/stb_image_write.h"

#include <iostream>
#include <cassert>
#include <fstream>
#include <execution>

#define SIZE_OF_FLOAT16 2

static uint32_t packOffset(uint32_t x, uint32_t y)
{
    uint32_t xOffset = x & 0x00ff;
    uint32_t yOffset = (y & 0x00ff) << 16;
    return xOffset | yOffset;
}

static size_t getChunkID(uint32_t offsetX, uint32_t offsetY, uint32_t mip)
{
    return hash(packOffset(offsetX, offsetY), mip);
}

static size_t getChunkID(uint32_t offset, uint32_t mip)
{
    return hash(offset, mip);
}

static void prepareImageLayout(std::shared_ptr<VulkanImage> src, std::shared_ptr<VulkanImage> dst)
{
    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.image = dst->getVkImage();
        imageMemoryBarrier.subresourceRange.aspectMask = dst->getSpecification().Aspect;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    //for (uint32_t mip = 0; mip < src->getSpecification().Mips; mip++)
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.image = src->getVkImage();
        imageMemoryBarrier.subresourceRange.aspectMask = src->getSpecification().Aspect;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = src->getSpecification().Mips;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    VkUtils::flushCommandBuffer(cmdBuffer);
}

static void restoreImageLayout(std::shared_ptr<VulkanImage> src, std::shared_ptr<VulkanImage> dst)
{
    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.image = dst->getVkImage();
        imageMemoryBarrier.subresourceRange.aspectMask = dst->getSpecification().Aspect;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    //for (uint32_t mip = 0; mip < src->getSpecification().Mips; mip++)
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.image = src->getVkImage();
        imageMemoryBarrier.subresourceRange.aspectMask = src->getSpecification().Aspect;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = src->getSpecification().Mips;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    VkUtils::flushCommandBuffer(cmdBuffer);
}

TerrainVirtualMap::TerrainVirtualMap(const VirtualTerrainMapSpecification& spec) : m_Specification(spec)
{
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

    int32_t availableSlots = m_Specification.PhysicalTextureSize / m_Specification.ChunkSize;
    availableSlots *= availableSlots;
    for (int32_t slot = 0; slot < availableSlots; slot++)
        m_AvailableSlots.insert(slot);

    m_LastSlotChunk.resize(availableSlots);
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
            int32_t avSlot = m_LastChunkSlot[chunkHashValue];
            if (m_LastSlotChunk[avSlot] == chunkHashValue)
            {
                m_AvailableSlots.erase(avSlot);
                m_ActiveNodes.insert(chunkHashValue);
            }
            else
                DynamicVirtualTerrainDeserializer::Get()->pushLoadTask(this, chunkHashValue, m_Specification.Filepath, m_ChunkProperties[chunkHashValue]);
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

void TerrainVirtualMap::blitNode(NodeData& nodeData, VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanBuffer>& StagingBuffer)
{
    if (m_ActiveNodes.find(nodeData.Node) != m_ActiveNodes.end())
        return;
    m_ActiveNodes.insert(nodeData.Node);

    VkImageSubresourceRange imgSubresource{};
    imgSubresource.aspectMask = m_PhysicalTexture->getSpecification().Aspect;
    imgSubresource.layerCount = 1;
    imgSubresource.levelCount = 1;
    imgSubresource.baseMipLevel = 0;

    int32_t iChunkSize = (int32_t)m_Specification.ChunkSize;

    assert(m_AvailableSlots.size() != 0);

    int32_t availableLoc = *m_AvailableSlots.begin();
    m_AvailableSlots.erase(availableLoc);

    // Cache
    m_LastChunkSlot[nodeData.Node] = availableLoc;
    m_LastSlotChunk[availableLoc] = nodeData.Node;

    int32_t slotsPerRow = m_Specification.PhysicalTextureSize / m_Specification.ChunkSize;
    int32_t x = availableLoc / slotsPerRow;
    int32_t y = availableLoc % slotsPerRow;

    VkUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_PhysicalTexture->copyBuffer(cmdBuffer, *StagingBuffer->getBaseBuffer(), 0, glm::uvec2(m_Specification.ChunkSize, m_Specification.ChunkSize), 
        glm::uvec2(x * iChunkSize, y * iChunkSize));
    VkUtils::transitionImageLayout(cmdBuffer, m_PhysicalTexture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
}

void TerrainVirtualMap::addChunkProperty(size_t chunk, const OnFileChunkProperties& prop)
{
    m_ChunkProperties[chunk] = prop;
}

// =============================== SERIALIZER ===============================

void VirtualTerrainSerializer::Serialize(const std::shared_ptr<VulkanImage>& map, const VirtualTerrainMapSpecification& spec,
    glm::uvec2 worldOffset, bool purgeContent)
{
    std::shared_ptr<VulkanImage> auxImg;
    {
        VulkanImageSpecification auxSpecification{};
        auxSpecification.Width = spec.ChunkSize;
        auxSpecification.Height = spec.ChunkSize;
        auxSpecification.Format = map->getSpecification().Format;
        auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
        auxSpecification.Mips = 1;

        auxImg = std::make_shared<VulkanImage>(auxSpecification);
        auxImg->Create();
    }

    VkDevice device = VulkanDevice::getVulkanDevice();

    VkImageSubresourceRange imgSubresource{};
    VulkanImageSpecification texSpec = map->getSpecification();
    imgSubresource.aspectMask = texSpec.Aspect;
    imgSubresource.layerCount = texSpec.LayerCount;
    imgSubresource.levelCount = texSpec.Mips;
    imgSubresource.baseMipLevel = 0;

    // Generate mips for heightmap
    VkUtils::transitionImageLayout(map->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    map->generateMips();

    prepareImageLayout(map, auxImg);

    {
        // Map memory once
        VkDeviceMemory memory = auxImg->getVkDeviceMemory();
        const char* data = nullptr;

        VkResult result = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
        if (result != VK_SUCCESS) {
            std::cout << "Failed to map Vulkan memory: " << result << std::endl;
            return;
        }

        VkImageSubresource subresource = {};
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkSubresourceLayout layout;
        vkGetImageSubresourceLayout(device, auxImg->getVkImage(), &subresource, &layout);

        const char* imageData = data + layout.offset;

        std::ofstream tableOut;
        std::ofstream imgCacheOut;

        if (purgeContent)
        {
            tableOut = std::ofstream(spec.Filepath.Table, std::ios::trunc);
            imgCacheOut = std::ofstream(spec.Filepath.Data, std::ios::binary | std::ios::trunc);
        }
        else
        {
            tableOut = std::ofstream(spec.Filepath.Table, std::ios::app);
            imgCacheOut = std::ofstream(spec.Filepath.Data, std::ios::binary | std::ios::app);
        }

        uint32_t size = spec.ChunkSize * spec.ChunkSize * SIZE_OF_FLOAT16;
        size_t binOffset = 0;

        // Serialize each mip 
        for (uint32_t mip = 0; mip < spec.LODCount; mip++)
        {
            uint32_t currentSize = map->getSpecification().Width >> mip;

            for (int32_t y = 0; y < currentSize / spec.ChunkSize; y++)
                for (int32_t x = 0; x < currentSize / spec.ChunkSize; x++)
                {
                    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

                    // Blit portion of heightmap to dstImage
                    VkImageBlit blit{};

                    int32_t iChunkSize = spec.ChunkSize;

                    blit.srcOffsets[0] = { x * iChunkSize, y * iChunkSize, 0 };
                    blit.srcOffsets[1] = { (x + 1) * iChunkSize, (y + 1) * iChunkSize, 1 };
                    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.srcSubresource.mipLevel = mip;
                    blit.srcSubresource.baseArrayLayer = 0;
                    blit.srcSubresource.layerCount = 1;
                    blit.dstOffsets[0] = { 0, 0, 0 };
                    blit.dstOffsets[1] = { iChunkSize, iChunkSize, 1 };
                    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.dstSubresource.mipLevel = 0;
                    blit.dstSubresource.baseArrayLayer = 0;
                    blit.dstSubresource.layerCount = 1;

                    vkCmdBlitImage(cmdBuffer,
                        map->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        auxImg->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit,
                        VK_FILTER_LINEAR);

                    VkUtils::flushCommandBuffer(cmdBuffer);

                    // Serialize to file
                    uint32_t worldOffsetPacked = packOffset(worldOffset.x + x, worldOffset.y + y);

                    tableOut << size << " ";
                    tableOut << mip << " ";
                    tableOut << worldOffsetPacked << " ";
                    tableOut << binOffset << " ";

                    binOffset += size;

                    imgCacheOut.write(imageData, size);
                }
        }

        vkUnmapMemory(device, memory);
    }

    restoreImageLayout(map, auxImg);
}

void VirtualTerrainSerializer::Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap)
{
    std::ifstream tabCache = std::ifstream(virtualMap->getSpecification().Filepath.Table);
    uint32_t size, mip, worldOffset;
    size_t binOffset;

    while (tabCache >> size >> mip >> worldOffset >> binOffset)
        virtualMap->addChunkProperty(getChunkID(worldOffset, mip), { binOffset, size });
}

// =============================== DYNAMIC DESERIALIZER ===============================
DynamicVirtualTerrainDeserializer* DynamicVirtualTerrainDeserializer::m_Instance = nullptr;

void DynamicVirtualTerrainDeserializer::Initialize()
{
    m_BufferPool.resize(LOAD_BATCH_SIZE);
    for (std::shared_ptr<VulkanBuffer>& buffer : m_BufferPool)
        buffer = std::make_shared<VulkanBuffer>(128 * 128 * 2, BufferType::TRANSFER_SRC, BufferUsage::DYNAMIC);

    m_LoadThread = std::thread([&]() -> void {
        while (m_ThreadRunning)
        {
            if (m_LoadTasks.empty())
                continue;
    
            ChunkLoadTask task;
            {
                std::lock_guard<std::mutex> lock(m_TaskMutex);
                task = m_LoadTasks.front();
                m_LoadTasks.pop();
            }
    
            loadChunk(task);
        }
        });
}

void DynamicVirtualTerrainDeserializer::Shutdown()
{
    m_ThreadRunning = false;
    m_LoadThread.join();

    m_BufferPool.clear();

    for (auto& fileCache : m_FileHandlerCache)
        delete fileCache.second;

    m_FileHandlerCache.clear();
}

void DynamicVirtualTerrainDeserializer::pushLoadTask(TerrainVirtualMap* vm, size_t node, const VirtualTextureLocation& location, OnFileChunkProperties onFileProps)
{
    std::lock_guard<std::mutex> lock(m_TaskMutex);
    m_LoadTasks.push({ vm, node, location, onFileProps });
}

void DynamicVirtualTerrainDeserializer::loadChunk(const ChunkLoadTask& task)
{
    if (m_FileHandlerCache.find(task.Location.Data) == m_FileHandlerCache.end())
        m_FileHandlerCache[task.Location.Data] = new std::ifstream(task.Location.Data, std::ios::binary);

    if (!m_FileHandlerCache[task.Location.Data]->is_open())
        throw(false);

    NodeToBlit ntb;
    ntb.Node.Data.resize(task.OnFileProperties.Size);
    ntb.Node.Node = task.Node;
    ntb.Destination = task.VirtualMap;

    {
        m_FileHandlerCache[task.Location.Data]->seekg(task.OnFileProperties.Offset, std::ios::beg);
        m_FileHandlerCache[task.Location.Data]->read(ntb.Node.Data.data(), task.OnFileProperties.Size);
    }

    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_NodesToBlit.push_back(ntb);
}

void DynamicVirtualTerrainDeserializer::Refresh()
{
    // I really need to multithread this :-?
    /*while (!m_LoadTasks.empty())
    {
        ChunkLoadTask task;
        {
            std::lock_guard<std::mutex> lock(m_TaskMutex);
            task = m_LoadTasks.front();
            m_LoadTasks.pop();
        }

        loadChunk(task);
    }*/
    std::lock_guard<std::mutex> lock(m_DataMutex);

    for (uint32_t batch = 0; batch <= m_NodesToBlit.size() / LOAD_BATCH_SIZE; batch++)
    {
        VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

        uint32_t endIndex = std::min(batch * LOAD_BATCH_SIZE,(uint32_t) m_NodesToBlit.size());

        for (uint32_t ndIndex = 0; ndIndex < endIndex; ndIndex++)
        {
            uint32_t bufferIndex = ndIndex % LOAD_BATCH_SIZE;

            auto& nd = m_NodesToBlit[ndIndex];

            uint32_t chunkSize = nd.Destination->getSpecification().ChunkSize;

            m_BufferPool[bufferIndex]->Map();
            m_BufferPool[bufferIndex]->setDataDirect(nd.Node.Data.data(), chunkSize * chunkSize * SIZE_OF_FLOAT16);
            m_BufferPool[bufferIndex]->Unmap();

            nd.Destination->blitNode(nd.Node, cmdBuffer, m_BufferPool[bufferIndex]);
        }

        VkUtils::endSingleTimeCommand(cmdBuffer);
    }

    uint32_t index = 0;

    m_NodesToBlit.clear();
}

