#include "VirtualTerrainSerializer.h"

#include <iostream>

#include "VirtualMapData.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Core/Hash.h"
#include "Core/DCompressor.h"

#include <zstd.h>

#include <Terrain/Terrain.h>
#include "Terrain/TerrainChunk.h"

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

void generateImageMips(const std::shared_ptr<VulkanImage>& image)
{
    if (image == nullptr)
        return;
    
    VkImageSubresourceRange imgSubresource{};
    VulkanImageSpecification texSpec = image->getSpecification();
    imgSubresource.aspectMask = texSpec.Aspect;
    imgSubresource.layerCount = texSpec.LayerCount;
    imgSubresource.levelCount = texSpec.Mips;
    imgSubresource.baseMipLevel = 0;

    // Generate mips for heightmap
    VkUtils::transitionImageLayout(image->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    image->generateMips(VK_FILTER_NEAREST);
}


static void prepareImageLayout(VkCommandBuffer cmdBuffer, const std::shared_ptr<VulkanImage>& image)
{
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.image = image->getVkImage();
    imageMemoryBarrier.subresourceRange.aspectMask = image->getSpecification().Aspect;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void mapAuxImage(const std::shared_ptr<VulkanImage>& image, const char*& data, VkSubresourceLayout& layout)
{
    VkDevice device = VulkanDevice::getVulkanDevice();

    VkDeviceMemory memory = image->getVkDeviceMemory();

    VkResult result = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    if (result != VK_SUCCESS) {
        std::cout << "Failed to map Vulkan memory: " << result << std::endl;
        return;
    }

    VkImageSubresource subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkGetImageSubresourceLayout(device, image->getVkImage(), &subresource, &layout);
}

void VirtualTerrainSerializer::Serialize(const SerializeSettings& settings)
{
    generateImageMips(settings.heightMap);
    generateImageMips(settings.normalMap);
    generateImageMips(settings.compositionMap);

    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();
    prepareImageLayout(cmdBuffer, settings.heightMap);
    prepareImageLayout(cmdBuffer, settings.normalMap);
    prepareImageLayout(cmdBuffer, settings.compositionMap);
    VkUtils::endSingleTimeCommand(cmdBuffer);
    
    uint32_t paddingChunkSize = settings.chunkSize + 2;

    VulkanImageSpecification auxSpecification{};
    auxSpecification.Width = paddingChunkSize;
    auxSpecification.Height = paddingChunkSize;
    auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
    auxSpecification.Mips = 1;

    auxSpecification.Format = settings.heightMap->getSpecification().Format;
    std::shared_ptr<VulkanImage> heightMapAux = std::make_shared<VulkanImage>(auxSpecification);

    auxSpecification.Format = settings.normalMap->getSpecification().Format;
    std::shared_ptr<VulkanImage> normalMapAux = std::make_shared<VulkanImage>(auxSpecification);

    auxSpecification.Format = settings.compositionMap->getSpecification().Format;
    std::shared_ptr<VulkanImage> compositionMapAux = std::make_shared<VulkanImage>(auxSpecification);
    
    size_t binaryOffset = 0;

    std::ofstream metadataOut = std::ofstream(settings.metadataFilepath, std::ios::trunc);
    std::ofstream rawdataOut = std::ofstream(settings.rawdataFilepath, std::ios::binary | std::ios::trunc);

    const char* heightMapData = nullptr;
    const char* normalMapData = nullptr;
    const char* compositionMapData = nullptr;

    VkSubresourceLayout heightMapLayout{};
    VkSubresourceLayout normalMapLayout{};
    VkSubresourceLayout compositionMapLayout{};

    mapAuxImage(heightMapAux, heightMapData, heightMapLayout);
    mapAuxImage(normalMapAux, normalMapData, normalMapLayout);
    mapAuxImage(compositionMapAux, compositionMapData, compositionMapLayout);

    metadataOut << "heightMap " << paddingChunkSize * paddingChunkSize * SIZE_OF_FLOAT16 << "\n";
    metadataOut << "normalMap " << paddingChunkSize * paddingChunkSize * 4 << "\n"; // normalMap type: rgba8, 4 bytes, 32 bits
    metadataOut << "compositionMap " << paddingChunkSize * paddingChunkSize * 1 << "\n"; // compositionMap type r8ui, 1 byte, 8 bits

    for (uint32_t mip = 0; mip < settings.mips; mip++)
    {
        uint32_t currentSize = settings.textureSize >> mip;

        for (int32_t y = 0; y < currentSize / settings.chunkSize; y++)
            for (int32_t x = 0; x < currentSize / settings.chunkSize; x++)
            {
                // wipe memory
                {
                    const char* hmData = heightMapData + heightMapLayout.offset;
                    memset((void*)hmData, 0, normalMapLayout.size);

                    const char* nData = normalMapData + normalMapLayout.offset;
                    memset((void*)nData, 0, normalMapLayout.size);

                    const char* cData = compositionMapData + normalMapLayout.offset;
                    memset((void*)cData, 0, normalMapLayout.size);
                }

                VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();
                VkImageBlit blit{};

                int32_t iChunkSize = settings.chunkSize;

                blit.srcOffsets[0] = { glm::max(x * iChunkSize - 1, 0), glm::max(y * iChunkSize - 1, 0), 0 };
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
                    settings.heightMap->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    heightMapAux->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_NEAREST);

                vkCmdBlitImage(cmdBuffer,
                    settings.normalMap->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    normalMapAux->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_NEAREST);

                vkCmdBlitImage(cmdBuffer,
                    settings.compositionMap->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    compositionMapAux->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_NEAREST);

                VkUtils::flushCommandBuffer(cmdBuffer);
            }
    }
}

// The virtual map may be bigger than we can fit in memory, we need to combine multiple data from the file into one image
void VirtualTerrainSerializer::Serialize(const std::shared_ptr<VulkanImage>& texture, const std::string& table, const std::string& datafile, uint32_t chunkSize,
    glm::uvec2 worldOffset, bool purgeContent)
{
    VkDevice device = VulkanDevice::getVulkanDevice();

    VkImageSubresourceRange imgSubresource{};
    VulkanImageSpecification texSpec = texture->getSpecification();
    imgSubresource.aspectMask = texSpec.Aspect;
    imgSubresource.layerCount = texSpec.LayerCount;
    imgSubresource.levelCount = texSpec.Mips;
    imgSubresource.baseMipLevel = 0;

    // Generate mips for heightmap
    VkUtils::transitionImageLayout(texture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture->generateMips(VK_FILTER_NEAREST);

    std::shared_ptr<VulkanImage> auxImg;

    {
        VulkanImageSpecification auxSpecification{};
        auxSpecification.Width = chunkSize + 2;
        auxSpecification.Height = chunkSize + 2;
        auxSpecification.Format = VK_FORMAT_R16_SFLOAT;
        auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
        auxSpecification.Mips = 1;

        auxImg = std::make_shared<VulkanImage>(auxSpecification);
        auxImg->Create();
    }

    prepareImageLayout(texture, auxImg);

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

        if (purgeContent)
        {
            tableOut = std::ofstream(table, std::ios::trunc);
            imgCacheOut = std::ofstream(datafile, std::ios::binary | std::ios::trunc);
        }
        else
        {
            tableOut = std::ofstream(table, std::ios::app);
            imgCacheOut = std::ofstream(datafile, std::ios::binary | std::ios::app);
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(VulkanDevice::getVulkanDevice(), auxImg->getVkImage(), &memRequirements);

        uint32_t chunkSizePadding = chunkSize + 2;

        uint32_t size = chunkSizePadding * chunkSizePadding * 2;
        size_t binOffset = 0;

        // TODO: all
        // Serialize each mip 
        for (uint32_t mip = 0; mip < 4; mip++)
        {
            uint32_t currentSize = texture->getSpecification().Width >> mip;

            for (int32_t y = 0; y < currentSize / chunkSize; y++)
                for (int32_t x = 0; x < currentSize / chunkSize; x++)
                {
                    const char* imageData = data + layout.offset;
                    memset((void*)imageData, 0, layout.size);
                    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

                    // Blit portion of heightmap to dstImage
                    VkImageBlit blit{};

                    int32_t iChunkSize = chunkSize;

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
                        texture->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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

    restoreImageLayout(texture, auxImg);
}

void VirtualTerrainSerializer::SerializeEntireMap(const std::shared_ptr<VulkanImage>& texture, const std::string& path)
{
    std::ofstream imgCacheOut = std::ofstream(path, std::ios::binary | std::ios::trunc);

    VkDeviceMemory memory = texture->getVkDeviceMemory();
    char* data = nullptr;

    VkDevice device = VulkanDevice::getVulkanDevice();
    VkResult result = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    if (result != VK_SUCCESS) {
        std::cout << "Failed to map Vulkan memory: " << result << std::endl;
        return;
    }

    VkImageSubresource subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device, texture->getVkImage(), &subresource, &layout);

    char* imageData = data + layout.offset;

    uint32_t size = texture->getSpecification().Width;
    uint32_t textureSize = size * size * SIZE_OF_FLOAT16;

    imgCacheOut.write((char*)&textureSize, sizeof(uint32_t));

    for (uint32_t y1 = 0; y1 < size; y1++)
    {
        uint16_t* row = (uint16_t*)imageData;
        for (uint32_t x1 = 0; x1 < size; x1++)
        {
            imgCacheOut.write((char*)row, 2);
            row++;
        }
        imageData += layout.rowPitch;
    }
}

void VirtualTerrainSerializer::SerializeClipmap(const std::shared_ptr<VulkanImage>& texture, const std::string& table, const std::string& datafile, uint32_t chunkSize, glm::uvec2 worldOffset, bool purgeContent)
{
    VkDevice device = VulkanDevice::getVulkanDevice();

    VkImageSubresourceRange imgSubresource{};
    VulkanImageSpecification texSpec = texture->getSpecification();
    imgSubresource.aspectMask = texSpec.Aspect;
    imgSubresource.layerCount = texSpec.LayerCount;
    imgSubresource.levelCount = texSpec.Mips;
    imgSubresource.baseMipLevel = 0;

    // Generate mips for heightmap
    VkUtils::transitionImageLayout(texture->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture->generateMips(VK_FILTER_NEAREST);

    std::shared_ptr<VulkanImage> auxImg;

    {
        VulkanImageSpecification auxSpecification{};
        auxSpecification.Width = chunkSize;
        auxSpecification.Height = chunkSize;
        auxSpecification.Format = VK_FORMAT_R16_SFLOAT;
        auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
        auxSpecification.Mips = 1;

        auxImg = std::make_shared<VulkanImage>(auxSpecification);
        auxImg->Create();
    }

    prepareImageLayout(texture, auxImg);

    {
        // MEMORY ALIGMENT
        // Map memory once
        VkDeviceMemory memory = auxImg->getVkDeviceMemory();
        char* data = nullptr;

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

        if (purgeContent)
        {
            tableOut = std::ofstream(table, std::ios::trunc);
            imgCacheOut = std::ofstream(datafile, std::ios::binary | std::ios::trunc);
        }
        else
        {
            tableOut = std::ofstream(table, std::ios::app);
            imgCacheOut = std::ofstream(datafile, std::ios::binary | std::ios::app);
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(VulkanDevice::getVulkanDevice(), auxImg->getVkImage(), &memRequirements);

        uint32_t chunkSizePadding = chunkSize;

        uint32_t size = chunkSizePadding * chunkSizePadding * 2;
        size_t binOffset = 0;

        DCompressor comperssor("Resources/Compression/heightmapDictionary", 5);

        // TODO: all
        // Serialize each mip 
        for (uint32_t mip = 0; mip < 4; mip++)
        {
            uint32_t currentSize = texture->getSpecification().Width >> mip;

            for (int32_t y = 0; y < currentSize / chunkSize; y++)
                for (int32_t x = 0; x < currentSize / chunkSize; x++)
                {
                    char* imageData = data + layout.offset;
                    memset((void*)imageData, 0, layout.size);
                    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

                    // Blit portion of heightmap to dstImage
                    VkImageBlit blit{};

                    int32_t iChunkSize = chunkSize;

                    blit.srcOffsets[0] = { glm::max(x * iChunkSize, 0), glm::max(y * iChunkSize, 0), 0 };
                    blit.srcOffsets[1] = { glm::min((x + 1) * iChunkSize, int32_t(currentSize)), glm::min((y + 1) * iChunkSize, int32_t(currentSize)), 1 };
                    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.srcSubresource.mipLevel = mip;
                    blit.srcSubresource.baseArrayLayer = 0;
                    blit.srcSubresource.layerCount = 1;

                    blit.dstOffsets[0] = { 0, 0, 0 };
                    blit.dstOffsets[1] = { 128, 128, 1 };
                    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.dstSubresource.mipLevel = 0;
                    blit.dstSubresource.baseArrayLayer = 0;
                    blit.dstSubresource.layerCount = 1;

                    vkCmdBlitImage(cmdBuffer,
                        texture->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        auxImg->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit,
                        VK_FILTER_NEAREST);

                    VkUtils::flushCommandBuffer(cmdBuffer);

                    // Serialize to file
                    uint32_t worldOffsetPacked = packOffset(worldOffset.x + x, worldOffset.y + y);

                    tableOut << size << " ";
                    tableOut << mip << " ";
                    tableOut << worldOffsetPacked << " ";
                    tableOut << binOffset << " ";

                    binOffset += size;

                    for (uint32_t y1 = 0; y1 < 128; y1++)
                    {
                        uint16_t* row = (uint16_t*)imageData;
                        for (uint32_t x1 = 0; x1 < 128; x1++)
                        {
                            imgCacheOut.write((char*)row, 2);
                            row++;
                        }
                        imageData += layout.rowPitch;
                    }
                }
        }

        vkUnmapMemory(device, memory);
    }

    restoreImageLayout(texture, auxImg);
}
