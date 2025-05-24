#include "HeightMapDiskProcessor.h"

#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanUtils.h"

#include "Terrain/TerrainChunk.h"

#include "stb_image/stb_image.h"

#include <fstream>
#include <iostream>

#define SIZE_OF_FLOAT16 2

static void prepareImageLayout(std::shared_ptr<VulkanImage> src, std::shared_ptr<VulkanImage> dst)
{
    VkCommandBuffer cmdBuffer = VulkanUtils::beginSingleTimeCommand();
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
    VulkanUtils::flushCommandBuffer(cmdBuffer);
}

static void restoreImageLayout(std::shared_ptr<VulkanImage> src, std::shared_ptr<VulkanImage> dst)
{
    VkCommandBuffer cmdBuffer = VulkanUtils::beginSingleTimeCommand();
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
    VulkanUtils::flushCommandBuffer(cmdBuffer);
}


HeightMapDiskProcessor::HeightMapDiskProcessor(const std::string& filepath, uint32_t lods)
{
	uint16_t* pixels = stbi_load_16(filepath.c_str(), &m_Width, &m_Height, &m_Channels, 1);

	VulkanImageSpecification heightmapSpec;
	heightmapSpec.Width = m_Width;
	heightmapSpec.Height = m_Height;
	heightmapSpec.Format = VK_FORMAT_R16_UNORM;
	heightmapSpec.Mips = lods;
	heightmapSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
	heightmapSpec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	heightmapSpec.UsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	heightmapSpec.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	heightmapSpec.CreateSampler = false;
	heightmapSpec.LayerCount = 1;

	m_HeightMap = std::make_shared<VulkanImage>(heightmapSpec);
	m_HeightMap->Create();

	VkDeviceSize imageSize = m_Width * m_Height * m_Channels * SIZE_OF_FLOAT16;

	if (!pixels)
		assert(false);

	VulkanBufferProperties imageBufferProperties;
	imageBufferProperties.Size = (uint32_t)imageSize;
	imageBufferProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
	imageBufferProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

	std::shared_ptr<VulkanBuffer> stagingBuffer = std::make_shared<VulkanBuffer>(imageBufferProperties);

	stagingBuffer->Map();
	memcpy(stagingBuffer->getMappedData(), pixels, static_cast<size_t>(imageSize));
	stagingBuffer->Unmap();

	stbi_image_free(pixels);

	VkImageSubresourceRange imgSubresource{};
	VulkanImageSpecification texSpec = m_HeightMap->getSpecification();
	imgSubresource.aspectMask = texSpec.Aspect;
	imgSubresource.layerCount = 1;
	imgSubresource.levelCount = texSpec.Mips;
	imgSubresource.baseMipLevel = 0;

	VulkanUtils::transitionImageLayout(m_HeightMap->getVkImage(), imgSubresource, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	m_HeightMap->copyBuffer(*stagingBuffer);
	m_HeightMap->generateMips(VK_FILTER_NEAREST);
}

void HeightMapDiskProcessor::serializeChunked(const SerializeChunkedSettings& settings)
{
    VkDevice device = VulkanDevice::getVulkanDevice();

    uint32_t paddedChunkSize = settings.ChunkSize + 2;

    VulkanImageSpecification auxSpecification{};
    auxSpecification.Width = paddedChunkSize;
    auxSpecification.Height = paddedChunkSize;
    auxSpecification.Format = VK_FORMAT_R16_UNORM;
    auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
    auxSpecification.Mips = 1;

    std::shared_ptr<VulkanImage> terrainChunkImage = std::make_shared<VulkanImage>(auxSpecification);
    terrainChunkImage->Create();

    prepareImageLayout(m_HeightMap, terrainChunkImage);

    const char* data;
    
    // Map memory
    {
        VkResult result = vkMapMemory(device, terrainChunkImage->getVkDeviceMemory(), 0, VK_WHOLE_SIZE, 0, (void**)&data);
        if (result != VK_SUCCESS) {
            std::cout << "Failed to map Vulkan memory: " << result << std::endl;
            return;
        }
    }

    VkImageSubresource subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device, terrainChunkImage->getVkImage(), &subresource, &layout);

    uint32_t size = paddedChunkSize * paddedChunkSize * SIZE_OF_FLOAT16;
    size_t binOffset = 0;

    std::ofstream metadataOut = std::ofstream(settings.MetadataFilepath, std::ios::trunc);
    std::ofstream rawOut = std::ofstream(settings.RawdataFilepath, std::ios::binary | std::ios::trunc);

    for (uint32_t mip = 0; mip < settings.LODs; mip++)
    {
        uint32_t currentSize = m_HeightMap->getSpecification().Width >> mip;
        uint32_t mipSize = currentSize / settings.ChunkSize;

        for (int32_t y = 0; y < mipSize; y++)
            for (int32_t x = 0; x < mipSize; x++)
            {
                const char* imageData = data + layout.offset;
                memset((void*)imageData, 0, layout.size);
                VkCommandBuffer cmdBuffer = VulkanUtils::beginSingleTimeCommand();

                VkImageBlit blit{};

                int32_t iChunkSize = settings.ChunkSize;

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
                    m_HeightMap->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    terrainChunkImage->getVkImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_NEAREST);

                VulkanUtils::flushCommandBuffer(cmdBuffer);

                metadataOut << mip << " ";
                metadataOut << packOffset(x, y) << " ";
                metadataOut << binOffset << " ";

                binOffset += size;

                for (uint32_t y1 = 0; y1 < paddedChunkSize; y1++)
                {
                    uint16_t* row = (uint16_t*)imageData;
                    for (uint32_t x1 = 0; x1 < paddedChunkSize; x1++)
                    {
                        rawOut.write((char*)row, SIZE_OF_FLOAT16);
                        row++;
                    }
                    imageData += layout.rowPitch;
                }
            }
    }

    vkUnmapMemory(device, terrainChunkImage->getVkDeviceMemory());

    restoreImageLayout(m_HeightMap, terrainChunkImage);
}
