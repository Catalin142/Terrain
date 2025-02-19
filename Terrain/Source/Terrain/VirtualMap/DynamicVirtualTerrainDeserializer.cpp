#include "DynamicVirtualTerrainDeserializer.h"

#include "TerrainVirtualMap.h"
#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Core/Instrumentor.h"

#include <Graphics/Vulkan/VulkanDevice.h>

#define CHUNK_PADDING 2

DynamicVirtualTerrainDeserializer::DynamicVirtualTerrainDeserializer(const VirtualTerrainMapSpecification& spec, int32_t chunkSize, const std::string& filepath) 
    : m_VirtualMapSpecification(spec), m_ChunkSize(chunkSize), m_ChunksDataFilepath(filepath)
{
    uint32_t chunkSizeWithPadding = m_ChunkSize + CHUNK_PADDING;
    m_TextureDataStride = chunkSizeWithPadding * chunkSizeWithPadding * SIZE_OF_FLOAT16;

    {
        VulkanBufferProperties RawDataProperties;
        RawDataProperties.Size = m_TextureDataStride * MAX_CHUNKS_LOADING_PER_FRAME;
        RawDataProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
        RawDataProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

        for (uint32_t frame = 0; frame <= VulkanRenderer::getFramesInFlight(); frame++)
        {
            m_RawImageData.push_back(std::make_shared<VulkanBuffer>(RawDataProperties));
            m_RawImageData.back()->Map();
        }
    }

    m_RegionsToCopy.reserve(MAX_CHUNKS_LOADING_PER_FRAME);

    m_IndirectionNodes.reserve(MAX_CHUNKS_LOADING_PER_FRAME);
    m_StatusNodes.reserve(MAX_CHUNKS_LOADING_PER_FRAME * 2);

    m_LoadThread = std::thread([&]() -> void {
        while (m_ThreadRunning)
        {
            m_LoadThreadSemaphore.acquire();
            {
                std::unique_lock lock(m_DataMutex);
                m_PositionsCV.wait(lock, [this] { return m_MemoryIndex != MAX_CHUNKS_LOADING_PER_FRAME; });
            }

            VirtualMapLoadTask task;

            {
                std::lock_guard<std::mutex> lock(m_TaskMutex);

                if (m_LoadTasks.empty())
                    continue;

                task = m_LoadTasks.front();
                m_LoadTasks.pop();
            }

            loadChunk(task);
        }
        });
}

DynamicVirtualTerrainDeserializer::~DynamicVirtualTerrainDeserializer()
{
    m_ThreadRunning = false;
    m_LoadThreadSemaphore.release();
    m_LoadThread.join();

    for (auto& fileCache : m_FileHandlerCache)
        delete fileCache.second;

    m_FileHandlerCache.clear();
}

void DynamicVirtualTerrainDeserializer::pushLoadTask(size_t node, int32_t virtualSlot, const FileChunkProperties& properties)
{
    {
        std::lock_guard<std::mutex> lock(m_TaskMutex);
        m_LoadTasks.push(VirtualMapLoadTask{node, virtualSlot, properties});
        m_Available = false;
    }
    m_LoadThreadSemaphore.release();
}

static char* buffer = new char[130 * 130 * 2];

void DynamicVirtualTerrainDeserializer::loadChunk(VirtualMapLoadTask task)
{
    if (m_FileHandlerCache.find(m_ChunksDataFilepath) == m_FileHandlerCache.end())
        m_FileHandlerCache[m_ChunksDataFilepath] = new std::ifstream(m_ChunksDataFilepath, std::ios::binary);

    if (!m_FileHandlerCache[m_ChunksDataFilepath]->is_open())
        throw(false);

    m_FileHandlerCache[m_ChunksDataFilepath]->seekg(task.Properties.inFileOffset, std::ios::beg);
    m_FileHandlerCache[m_ChunksDataFilepath]->read(buffer, m_TextureDataStride);

    {
        std::lock_guard<std::mutex> lock(m_DataMutex);

        char* charData = (char*)m_RawImageData[m_AvailableBuffer]->getMappedData();
        std::memcpy(&charData[m_MemoryIndex * m_TextureDataStride], buffer, m_TextureDataStride);

        int32_t slotsPerRow = m_VirtualMapSpecification.PhysicalTextureSize / m_ChunkSize;
        int32_t x = task.VirtualSlot / slotsPerRow;
        int32_t y = task.VirtualSlot % slotsPerRow;

        m_RegionsToCopy.push_back(createRegion(task));
        m_IndirectionNodes.push_back(GPUIndirectionNode{ task.Properties.Position, packOffset(x, y), task.Properties.Mip });
        m_StatusNodes.push_back(GPUStatusNode(task.Properties.Position, task.Properties.Mip, 1));

        m_MemoryIndex++;
    }
}

void DynamicVirtualTerrainDeserializer::Refresh(VkCommandBuffer cmdBuffer, TerrainVirtualMap* virtualMap)
{
    LastUpdate.IndirectionNodes.clear();
    LastUpdate.StatusNodes.clear();

    uint32_t currentBuffer = 0;
    std::vector<VkBufferImageCopy> regions;

    {
        std::lock_guard<std::mutex> lock(m_DataMutex);

        if (m_RegionsToCopy.size() == 0)
            return;

        LastUpdate.IndirectionNodes = m_IndirectionNodes;
        LastUpdate.StatusNodes = m_StatusNodes;

        currentBuffer = m_AvailableBuffer++;
        m_AvailableBuffer %= (VulkanRenderer::getFramesInFlight() + 1);

        regions = m_RegionsToCopy;

        m_RegionsToCopy.clear();
        m_IndirectionNodes.clear();
        m_StatusNodes.clear();

        m_MemoryIndex = 0;
        m_PositionsCV.notify_all();
    }

    virtualMap->prepareForDeserialization(cmdBuffer);
    virtualMap->blitNodes(cmdBuffer, m_RawImageData[currentBuffer], regions);
    virtualMap->prepareForRendering(cmdBuffer);

    {
        std::lock_guard<std::mutex> lock(m_TaskMutex);
        if (m_LoadTasks.size() == 0)
            m_Available = true;
    }
}

VkBufferImageCopy DynamicVirtualTerrainDeserializer::createRegion(const VirtualMapLoadTask& task)
{
    int32_t iChunkSize = m_ChunkSize + CHUNK_PADDING;

    VkBufferImageCopy region{};
    region.bufferOffset = m_MemoryIndex * m_TextureDataStride;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    int32_t slotsPerRow = m_VirtualMapSpecification.PhysicalTextureSize / m_ChunkSize;
    int32_t x = task.VirtualSlot / slotsPerRow;
    int32_t y = task.VirtualSlot % slotsPerRow;

    region.imageOffset = { x * iChunkSize, y * iChunkSize, 0 };
    region.imageExtent = { (uint32_t)iChunkSize, (uint32_t)iChunkSize, 1 };

    return region;
}