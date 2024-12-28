#include "DynamicVirtualTerrainDeserializer.h"

#include "TerrainVirtualMap.h"
#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "VMUtils.h"
#include "Core/Instrumentor.h"

#include <Graphics/Vulkan/VulkanDevice.h>


DynamicVirtualTerrainDeserializer::DynamicVirtualTerrainDeserializer(const VirtualTerrainMapSpecification& spec) : m_VirtualMapSpecification(spec)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    uint32_t chunkSize = m_VirtualMapSpecification.ChunkSize + 2;
    m_TextureDataStride = chunkSize * chunkSize * 2;

    {
        VulkanBufferProperties RawDataProperties;
        RawDataProperties.Size = m_TextureDataStride * MAX_CHUNKS_LOADING_PER_FRAME;
        RawDataProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
        RawDataProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

        m_RawImageData = std::make_shared<VulkanBuffer>(RawDataProperties);

        m_RawImageData->Map();
    }

    m_RegionsToCopy.reserve(MAX_CHUNKS_LOADING_PER_FRAME);
    m_UsedIndices.reserve(MAX_CHUNKS_LOADING_PER_FRAME);

    m_IndirectionNodes.reserve(1024);
    m_StatusNodes.reserve(1024);

    m_MaxLoadSemaphore.release(MAX_CHUNKS_LOADING_PER_FRAME);

    for (uint32_t i = 0; i < MAX_CHUNKS_LOADING_PER_FRAME; i++)
        m_AvailableSlots.push(i);

    m_LoadThread = std::thread([&]() -> void {
        while (m_ThreadRunning)
        {
            m_LoadThreadSemaphore.acquire();

            LoadTask task;
            // the main thread pushes tasks
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

void DynamicVirtualTerrainDeserializer::pushLoadTask(size_t node, int32_t virtualSlot, const VirtualTerrainChunkProperties& properties, VirtualTextureType type)
{
    {
        std::lock_guard<std::mutex> lock(m_TaskMutex);
        m_LoadTasks.push(LoadTask{node, virtualSlot, properties, type });
    }
    m_LoadThreadSemaphore.release();
}

void DynamicVirtualTerrainDeserializer::loadChunk(LoadTask task)
{
    VirtualTextureLocation location = m_VirtualMapSpecification.Filepaths[task.Type];

    if (m_FileHandlerCache.find(location.Data) == m_FileHandlerCache.end())
        m_FileHandlerCache[location.Data] = new std::ifstream(location.Data, std::ios::binary);

    if (!m_FileHandlerCache[location.Data]->is_open())
        throw(false);

    uint32_t fileOffset = task.Properties.inFileOffset;

    uint32_t memoryIndex = -1;
    uint32_t availableSlots = 0;

    // wait until we have free slots
    m_MaxLoadSemaphore.acquire();

    {
        std::lock_guard<std::mutex> lock(m_SlotsMutex);
        memoryIndex = m_AvailableSlots.front();
        m_AvailableSlots.pop();
    }
    
    {
        m_FileHandlerCache[location.Data]->seekg(fileOffset, std::ios::beg);

        char* charData = (char*)m_RawImageData->getMappedData();
        m_FileHandlerCache[location.Data]->read(&charData[memoryIndex * m_TextureDataStride], m_TextureDataStride);
    }

    int32_t iChunkSize = m_VirtualMapSpecification.ChunkSize + 2;

    VkBufferImageCopy region{};
    region.bufferOffset = memoryIndex * m_TextureDataStride;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    int32_t slotsPerRow = m_VirtualMapSpecification.PhysicalTextureSize / m_VirtualMapSpecification.ChunkSize;
    int32_t x = task.VirtualSlot / slotsPerRow;
    int32_t y = task.VirtualSlot % slotsPerRow;
    
    region.imageOffset = { x * iChunkSize, y * iChunkSize, 0 };
    region.imageExtent = { (uint32_t)iChunkSize, (uint32_t)iChunkSize, 1 };

    {
        std::lock_guard<std::mutex> lock(m_DataMutex);

        m_UsedIndices.push_back(memoryIndex);
        m_RegionsToCopy.push_back(region);
        m_IndirectionNodes.push_back(GPUIndirectionNode{ task.Properties.Position, packOffset(x, y), task.Properties.Mip });
        m_StatusNodes.push_back(GPUStatusNode(task.Properties.Position, task.Properties.Mip, 1));
    }
}

void DynamicVirtualTerrainDeserializer::Refresh(VkCommandBuffer cmdBuffer, TerrainVirtualMap* virtualMap)
{
    std::vector<uint32_t> indicesCopy;
    std::vector<VkBufferImageCopy> regionsCopy;
    LastUpdate.IndirectionNodes.clear();
    LastUpdate.StatusNodes.clear();

    {
        std::lock_guard<std::mutex> lock(m_DataMutex);

        if (m_RegionsToCopy.size() == 0)
            return;

        indicesCopy = m_UsedIndices;
        regionsCopy = m_RegionsToCopy;

        LastUpdate.IndirectionNodes = m_IndirectionNodes;
        LastUpdate.StatusNodes = m_StatusNodes;

        m_UsedIndices.clear();
        m_RegionsToCopy.clear();
        m_IndirectionNodes.clear();
        m_StatusNodes.clear();
    }

    Instrumentor::Get().beginTimer("Refresh Deserializer");

    virtualMap->prepareForDeserialization(cmdBuffer);
    virtualMap->blitNodes(cmdBuffer, m_RawImageData, regionsCopy);
    virtualMap->prepareForRendering(cmdBuffer);

    for (uint32_t index : indicesCopy)
    {
        {
            std::lock_guard<std::mutex> lock(m_SlotsMutex);
            m_AvailableSlots.push(index);
        }
        m_MaxLoadSemaphore.release();
    }
    
    Instrumentor::Get().endTimer("Refresh Deserializer");
}