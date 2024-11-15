#include "DynamicVirtualTerrainDeserializer.h"

#include "Graphics/Vulkan/VulkanUtils.h"
#include "VMUtils.h"
#include <Graphics/Vulkan/VulkanDevice.h>

DynamicVirtualTerrainDeserializer* DynamicVirtualTerrainDeserializer::m_Instance = nullptr;

void DynamicVirtualTerrainDeserializer::Initialize(const std::shared_ptr<TerrainVirtualMap>& vm)
{
    m_VirtualMap = vm;

    {
        VulkanImageSpecification auxSpecification{};
        auxSpecification.Width = 130;
        auxSpecification.Height = 130;
        auxSpecification.Format = VK_FORMAT_R16_SFLOAT;
        auxSpecification.UsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        auxSpecification.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        auxSpecification.MemoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        auxSpecification.Tiling = VK_IMAGE_TILING_LINEAR;
        auxSpecification.Mips = 1;

        auxImg = std::make_shared<VulkanImage>(auxSpecification);
        auxImg->Create();
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(VulkanDevice::getVulkanDevice(), auxImg->getVkImage(), &memRequirements);
    VkDeviceSize imageSize = memRequirements.size;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(VulkanDevice::getVulkanDevice(), &semaphoreCreateInfo, nullptr, &hahah)) assert(false);

    uint32_t chunkSize = m_VirtualMap->getSpecification().ChunkSize;
    m_TextureDataStride = 130 * 130 * 2;

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

// TODO: push in buffers and when we have 8 buffers, blit
// If less than 8 we need, idk well see
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
    // TODO (urgent): modify this, don t run an empty commabd buffer each frame

    VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    submitInfo.pSignalSemaphores = &hahah;
    submitInfo.signalSemaphoreCount = 1;

    m_VirtualMap->prepareForDeserialization(cmdBuffer);
    VkUtils::endSingleTimeCommand(cmdBuffer, submitInfo);

    if (m_NodesToBlit.size() == 0)
        return;

    // Maybe multithread this, one batch per thread?
    std::lock_guard<std::mutex> lock(m_DataMutex);

    std::vector<LoadedNode> nodes;

    for (uint32_t batch = 0; batch <= m_NodesToBlit.size() / LOAD_BATCH_SIZE; batch++)
    {
        VkCommandBuffer cmdBuffer = VkUtils::beginSingleTimeCommand();

        uint32_t endIndex = std::min(batch * LOAD_BATCH_SIZE + LOAD_BATCH_SIZE, (uint32_t)m_NodesToBlit.size());

        nodes.clear();

        for (uint32_t index = batch * LOAD_BATCH_SIZE; index < endIndex ;index++)
        {
            uint32_t bufferIndex = index % LOAD_BATCH_SIZE;

            NodeData& nd = m_NodesToBlit[index];

            m_BufferPool[bufferIndex]->Map();
            m_BufferPool[bufferIndex]->setDataDirect(&m_TextureDataBuffer[nd.MemoryIndex * m_TextureDataStride], 
                m_TextureDataStride);
            m_BufferPool[bufferIndex]->Unmap();

            uint32_t physicalLocation = m_VirtualMap->blitNode(nd.Node, cmdBuffer, m_BufferPool[bufferIndex]);

            auxImg->copyBuffer(cmdBuffer, *m_BufferPool[bufferIndex]->getBaseBuffer(), 0, glm::uvec2(130, 130),
                glm::uvec2(0, 0));
            
            {
                std::lock_guard<std::mutex> lock(m_SlotsMutex);
                m_AvailableSlots.push(nd.MemoryIndex);
                m_OccupiedSlots--;
            }

            nodes.push_back(LoadedNode{nd.VirtualLocation, physicalLocation, nd.Mip, 0 });
        }

        m_VirtualMap->updateIndirectionTexture(cmdBuffer, nodes);

        const uint64_t signalValue = 2; 
        const uint64_t waitValue = 0; 

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        submitInfo.pSignalSemaphores = &hahah;
        submitInfo.signalSemaphoreCount = 1;

        m_VirtualMap->prepareForRendering(cmdBuffer);

        VkUtils::endSingleTimeCommand(cmdBuffer, submitInfo);
    }


    m_NodesToBlit.clear();
}
