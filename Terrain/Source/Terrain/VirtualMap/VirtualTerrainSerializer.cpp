#include "VirtualTerrainSerializer.h"

#include <iostream>

#include "VMUtils.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Core/Hash.h"

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