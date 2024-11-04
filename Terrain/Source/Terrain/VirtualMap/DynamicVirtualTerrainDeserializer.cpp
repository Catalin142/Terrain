#include "DynamicVirtualTerrainDeserializer.h"

#include "Graphics/Vulkan/VulkanUtils.h"
#include "VMUtils.h"

DynamicVirtualTerrainDeserializer* DynamicVirtualTerrainDeserializer::m_Instance = nullptr;

void DynamicVirtualTerrainDeserializer::Initialize(const std::shared_ptr<TerrainVirtualMap>& vm)
{
    m_VirtualMap = vm;
    
    uint32_t chunkSize = m_VirtualMap->getSpecification().ChunkSize;
    m_TextureDataStride = chunkSize * chunkSize * SIZE_OF_FLOAT16;

    // have room for 64 chunks at a time
    m_TextureDataBuffer = new char[m_TextureDataStride * MAX_CHUNKS_LOADING_PER_FRAME];
    for (uint32_t i = 0; i < MAX_CHUNKS_LOADING_PER_FRAME; i++)
        m_AvailableSlots.push(i);

    m_BufferPool.resize(LOAD_BATCH_SIZE);
    for (std::shared_ptr<VulkanBuffer>& buffer : m_BufferPool)
        buffer = std::make_shared<VulkanBuffer>(m_TextureDataStride, BufferType::TRANSFER_SRC, BufferUsage::DYNAMIC);

    m_LoadThread = std::thread([&]() -> void {
        while (m_ThreadRunning)
        {
            NodeData task;
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

void DynamicVirtualTerrainDeserializer::Shutdown()
{
    m_ThreadRunning = false;
    m_LoadThread.join();

    m_BufferPool.clear();

    for (auto& fileCache : m_FileHandlerCache)
        delete fileCache.second;

    m_FileHandlerCache.clear();

    delete[] m_TextureDataBuffer;
}

void DynamicVirtualTerrainDeserializer::pushLoadTask(size_t node, uint32_t virtualLocation, uint32_t mip, VirtualTextureType type)
{
    std::lock_guard<std::mutex> lock(m_TaskMutex);
    m_LoadTasks.push({ node, virtualLocation, mip, type });
}

void DynamicVirtualTerrainDeserializer::loadChunk(NodeData task)
{
    VirtualTextureLocation location = m_VirtualMap->getTypeLocation(task.Type);

    if (m_FileHandlerCache.find(location.Data) == m_FileHandlerCache.end())
        m_FileHandlerCache[location.Data] = new std::ifstream(location.Data, std::ios::binary);

    if (!m_FileHandlerCache[location.Data]->is_open())
        throw(false);

    uint32_t fileOffset = m_VirtualMap->getChunkFileOffset(task.Node);

    {
        std::lock_guard<std::mutex> lock(m_SlotsMutex);
        // at this stage, small map, we won't ever load that many chunks at the time
        // when we have a bigger map, don t forget to fix this edge case
        while (m_AvailableSlots.empty())
        {
            assert(false);
        }

        task.MemoryIndex = m_AvailableSlots.front();
        m_AvailableSlots.pop();
        m_OccupiedSlots++;
    }
    m_FileHandlerCache[location.Data]->seekg(fileOffset, std::ios::beg);
    m_FileHandlerCache[location.Data]->read(&m_TextureDataBuffer[task.MemoryIndex * m_TextureDataStride], m_TextureDataStride);

    std::lock_guard<std::mutex> lock(m_DataMutex);
    m_NodesToBlit.push_back(task);
}

void DynamicVirtualTerrainDeserializer::Refresh()
{
    if (m_NodesToBlit.size() == 0)
        return;

    // Maybe multithread this, one batch per thread?
    std::lock_guard<std::mutex> lock(m_DataMutex);

    std::vector<LoadedNode> nodes;

    for (uint32_t batch = 0; batch <= m_NodesToBlit.size() / LOAD_BATCH_SIZE; batch++)
    {
        VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

        uint32_t endIndex = std::min(batch * LOAD_BATCH_SIZE + LOAD_BATCH_SIZE, (uint32_t)m_NodesToBlit.size());

        for (uint32_t index = batch * LOAD_BATCH_SIZE; index < endIndex ; index++)
        {
            uint32_t bufferIndex = index % LOAD_BATCH_SIZE;

            NodeData& nd = m_NodesToBlit[index];

            m_BufferPool[bufferIndex]->Map();
            m_BufferPool[bufferIndex]->setDataDirect(&m_TextureDataBuffer[nd.MemoryIndex * m_TextureDataStride], 
                m_TextureDataStride);
            m_BufferPool[bufferIndex]->Unmap();

            uint32_t physicalLocation = m_VirtualMap->blitNode(nd.Node, cmdBuffer, m_BufferPool[bufferIndex]);

            {
                std::lock_guard<std::mutex> lock(m_SlotsMutex);
                m_AvailableSlots.push(nd.MemoryIndex);
                m_OccupiedSlots--;
            }

            nodes.push_back(LoadedNode{nd.VirtualLocation, physicalLocation, nd.Mip, 0 });
        }

        VkUtils::endSingleTimeCommand(cmdBuffer);
    }

    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();
    m_VirtualMap->updateIndirectionTexture(cmdBuffer, nodes);
    VkUtils::endSingleTimeCommand(cmdBuffer);

    m_NodesToBlit.clear();
}
