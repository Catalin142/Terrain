#include "VirtualTerrainSerializer.h"

#include <iostream>

#include "VMUtils.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Core/Hash.h"
#include <Terrain/Terrain.h>

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

// The virtual map may be bigger than we can fit in memory, we need to combine multiple data from the file into one image
void VirtualTerrainSerializer::Serialize(const std::shared_ptr<VulkanImage>& terrain, const VirtualTerrainMapSpecification& spec,
    VirtualTextureType type, glm::uvec2 worldOffset, bool purgeContent)
{
    VkDevice device = VulkanDevice::getVulkanDevice();

    VkImageSubresourceRange imgSubresource{};
    VulkanImageSpecification texSpec = terrain->getSpecification();
    imgSubresource.aspectMask = texSpec.Aspect;
    imgSubresource.layerCount = texSpec.LayerCount;
    imgSubresource.levelCount = texSpec.Mips;
    imgSubresource.baseMipLevel = 0;

    // Generate mips for heightmap
    VkUtils::transitionImageLayout(terrain->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    terrain->generateMips();

    std::shared_ptr<VulkanImage> auxImg;

    {
        VulkanImageSpecification auxSpecification{};
        auxSpecification.Width = 128 + 2;
        auxSpecification.Height = 128 + 2;
        auxSpecification.Format = VK_FORMAT_R16_SFLOAT;
        auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
        auxSpecification.Mips = 1;

        auxImg = std::make_shared<VulkanImage>(auxSpecification);
        auxImg->Create();
    }

    prepareImageLayout(terrain, auxImg);

    {
        // MEMORY ALIGMENT
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

        std::ofstream tableOut;
        std::ofstream imgCacheOut;

        VirtualTextureLocation filepath = spec.Filepaths.at(type);

        if (purgeContent)
        {
            tableOut = std::ofstream(filepath.Table, std::ios::trunc);
            imgCacheOut = std::ofstream(filepath.Data, std::ios::binary | std::ios::trunc);
        }
        else
        {
            tableOut = std::ofstream(filepath.Table, std::ios::app);
            imgCacheOut = std::ofstream(filepath.Data, std::ios::binary | std::ios::app);
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(VulkanDevice::getVulkanDevice(), auxImg->getVkImage(), &memRequirements);
        uint32_t size = 130 * 130 * 2;
        size_t binOffset = 0;

        // TODO: all
        // Serialize each mip 
        for (uint32_t mip = 0; mip < 4; mip++)
        {
            uint32_t currentSize = terrain->getSpecification().Width >> mip;

            for (int32_t y = 0; y < currentSize / spec.ChunkSize; y++)
                for (int32_t x = 0; x < currentSize / spec.ChunkSize; x++)
                {
                    const char* imageData = data + layout.offset;
                    memset((void*)imageData, 0, layout.size);
                    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

                    // Blit portion of heightmap to dstImage
                    VkImageBlit blit{};

                    int32_t iChunkSize = spec.ChunkSize;

                    blit.srcOffsets[0] = { glm::max(x * iChunkSize - 1, 0), glm::max(y * iChunkSize - 1, 0), 0};
                    blit.srcOffsets[1] = { glm::min((x + 1) * iChunkSize + 1, int32_t(currentSize)), glm::min((y + 1) * iChunkSize + 1, int32_t(currentSize)), 1 };
                    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.srcSubresource.mipLevel = mip;
                    blit.srcSubresource.baseArrayLayer = 0;
                    blit.srcSubresource.layerCount = 1;

                    int32_t startX1 = 0, startY1 = 0;
                    int32_t startX2 = iChunkSize + 2, startY2 = iChunkSize + 2;

                    if (x * iChunkSize - 1 < 0)
                        startX1 = 1;
                    
                    if (y * iChunkSize - 1 < 0)
                        startY1 = 1;

                    if ((x + 1) * iChunkSize + 1 > currentSize)
                        startX2 = iChunkSize + 1;
                    
                    if ((y + 1) * iChunkSize + 1 > currentSize)
                        startY2 = iChunkSize + 1;

                    blit.dstOffsets[0] = { startX1, startY1, 0 };
                    blit.dstOffsets[1] = { startX2, startY2, 1 };
                    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.dstSubresource.mipLevel = 0;
                    blit.dstSubresource.baseArrayLayer = 0;
                    blit.dstSubresource.layerCount = 1;

                    vkCmdBlitImage(cmdBuffer,
                        terrain->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        auxImg->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit,
                        VK_FILTER_NEAREST);

                    VkUtils::flushCommandBuffer(cmdBuffer);

                    // Serialize to file
                    uint32_t worldOffsetPacked = packOffset(worldOffset.x + x, worldOffset.y + y);

                    // TODO: don t write size in file, just at the beggining maybe
                    tableOut << size << " ";
                    tableOut << mip << " ";
                    tableOut << worldOffsetPacked << " ";
                    tableOut << binOffset << " ";

                    binOffset += size;

                    for (uint32_t y1 = 0; y1 < 130; y1++)
                    {
                        uint16_t* row = (uint16_t*)imageData;
                        for (uint32_t x1 = 0; x1 < 130; x1++)
                        {
                            imgCacheOut.write((char*)row, 2);
                            row++;
                        }
                        imageData += layout.rowPitch;
                    }
                    //imgCacheOut.write(imageData, size);
                }
        }

        vkUnmapMemory(device, memory);
    }

    restoreImageLayout(terrain, auxImg);
}

void VirtualTerrainSerializer::Deserialize(const std::shared_ptr<TerrainVirtualMap>& virtualMap, VirtualTextureType type)
{
    VirtualTextureLocation filepath = virtualMap->getSpecification().Filepaths.at(type);

    std::ifstream tabCache = std::ifstream(filepath.Table);
    uint32_t size, mip, worldOffset;
    size_t binOffset;

    while (tabCache >> size >> mip >> worldOffset >> binOffset)
        virtualMap->addVirtualChunkProperty(getChunkID(worldOffset, mip), { worldOffset, mip, binOffset });
}