#include "DynamicVirtualTerrainDeserializer.h"

#include "Graphics/Vulkan/VulkanUtils.h"
#include "VMUtils.h"

DynamicVirtualTerrainDeserializer* DynamicVirtualTerrainDeserializer::m_Instance = nullptr;

void DynamicVirtualTerrainDeserializer::Initialize()
{
    m_BufferPool.resize(LOAD_BATCH_SIZE);
    for (std::shared_ptr<VulkanBuffer>& buffer : m_BufferPool)
        buffer = std::make_shared<VulkanBuffer>(128 * 128 * 2, BufferType::TRANSFER_SRC, BufferUsage::DYNAMIC);

    m_LoadThread = std::thread([&]() -> void {
        while (m_ThreadRunning)
        {
            ChunkLoadTask task;
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
    // Maybe multithread this, one batch per thread?
    std::lock_guard<std::mutex> lock(m_DataMutex);

    for (uint32_t batch = 0; batch <= m_NodesToBlit.size() / LOAD_BATCH_SIZE; batch++)
    {
        VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

        uint32_t endIndex = std::min(batch * LOAD_BATCH_SIZE, (uint32_t)m_NodesToBlit.size());

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
