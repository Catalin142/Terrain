#include "DynamicClipmapDeserializer.h"

#include "Terrain/TerrainChunk.h"
#include "Graphics/Vulkan/VulkanRenderer.h"
#include "Terrain/Clipmap/TerrainClipmap.h"
#include "Terrain/Terrain.h"

#include "Core/DCompressor.h"

DynamicClipmapDeserializer::DynamicClipmapDeserializer(const ClipmapTerrainSpecification& spec, int32_t chunkSize, const std::string& filepath)
    : m_Specification(spec), m_ChunkSize(chunkSize), m_ChunksDataFilepath(filepath)
{
    int32_t chunkSizePadding = m_ChunkSize + 2;
    m_TextureDataStride = chunkSizePadding * chunkSizePadding * SIZE_OF_FLOAT16;

    {
        VulkanBufferProperties RawDataProperties;
        RawDataProperties.Size = m_TextureDataStride * MAX_CHUNKS_LOADING_PER_FRAME;
        RawDataProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
        RawDataProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

        for (uint32_t frame = 0; frame < VulkanRenderer::getFramesInFlight(); frame++)
        {
            m_RawImageData.push_back(std::make_shared<VulkanBuffer>(RawDataProperties));
            m_RawImageData.back()->Map();
        }
    }

    m_RegionsToCopy.reserve(MAX_CHUNKS_LOADING_PER_FRAME);

    m_LoadThread = std::thread([&]() -> void {
        while (m_ThreadRunning)
        {
            // block thead if no positions are queued
            {
                std::unique_lock lock(m_DataMutex);
                m_PositionsCV.wait(lock, [this] { return m_PositionsToProcess.size() != 0 || !m_ThreadRunning; });
            }

            ClipmapLoadTask task;

            {
                std::lock_guard lock(m_DataMutex);

                if (m_PositionsToProcess.empty())
                    continue;

                glm::ivec2 camPos = m_PositionsToProcess.front();
                size_t camHash = hash((uint32_t)camPos.x, (uint32_t)camPos.y);

                if (m_LoadTasks.empty() || m_LoadTasks.front().CameraHash != camHash)
                    continue;

                task = m_LoadTasks.front();
            }
            
            loadChunk(task.ChunkProperties);
            
            {
                std::lock_guard lock(m_DataMutex);
                m_LoadTasks.pop();
            }
        }
        });
}

DynamicClipmapDeserializer::~DynamicClipmapDeserializer()
{
    m_ThreadRunning = false;
    m_PositionsCV.notify_all();
    m_LoadThread.join();

    delete m_FileHandle;
}

void DynamicClipmapDeserializer::pushLoadChunk(const glm::ivec2& camPos, const FileChunkProperties& task)
{
    size_t camHash = hash((uint32_t)camPos.x, (uint32_t)camPos.y);

    {
        std::lock_guard lock(m_DataMutex);
        if (m_PositionsToProcess.empty() || m_PositionsToProcess.back() != camPos)
        {
            m_PositionsToProcess.push(camPos);
        
            // notify that we have positions to process
            m_PositionsCV.notify_all();
        }
        m_LoadTasks.push(ClipmapLoadTask{ camHash, task });
    }
}

void DynamicClipmapDeserializer::loadChunk(const FileChunkProperties& task)
{
    if (m_FileHandle == nullptr)
        m_FileHandle = new std::ifstream(m_ChunksDataFilepath, std::ios::binary);

    if (!m_FileHandle->is_open())
        throw(false);

    char* charData = nullptr;
    {
        std::lock_guard lock(m_DataMutex);
        charData = (char*)m_RawImageData[m_AvailableBuffer]->getMappedData();
    }

    m_FileHandle->seekg(task.inFileOffset, std::ios::beg);
    m_FileHandle->read(&charData[m_MemoryIndex * m_TextureDataStride], task.Size);

    {
        std::lock_guard lock(m_CopyDataMutex);
        m_RegionsToCopy.push_back(createRegion(task));
        m_MemoryIndex++;
    }
}

void DynamicClipmapDeserializer::loadChunkSequential(const FileChunkProperties& task)
{
    if (m_FileHandle == nullptr)
        m_FileHandle = new std::ifstream(m_ChunksDataFilepath, std::ios::binary);

    if (!m_FileHandle->is_open())
        throw(false);

    char* charData = (char*)m_RawImageData[0]->getMappedData();

    m_FileHandle->seekg(task.inFileOffset, std::ios::beg);
    m_FileHandle->read(&charData[m_MemoryIndex * m_TextureDataStride], task.Size);

    m_RegionsToCopy.push_back(createRegion(task));
    m_MemoryIndex++;
}

void DynamicClipmapDeserializer::Flush()
{
    m_RegionsToCopy.clear();
    m_MemoryIndex = 0;
}

bool DynamicClipmapDeserializer::Refresh(VkCommandBuffer cmdBuffer, TerrainClipmap* clipmap)
{
    glm::ivec2 camPos;
    size_t camHash;

    {
        std::lock_guard lock(m_DataMutex);

        if (m_PositionsToProcess.empty())
            return false;

        camPos = m_PositionsToProcess.front();
        camHash = hash((uint32_t)camPos.x, (uint32_t)camPos.y);

        if (!m_LoadTasks.empty() && m_LoadTasks.front().CameraHash == camHash)
            return false;
    }

    clipmap->prepareForDeserialization(cmdBuffer);
    clipmap->blitNodes(cmdBuffer, m_RawImageData[m_AvailableBuffer], m_RegionsToCopy);
    clipmap->prepareForRendering(cmdBuffer);

    {
        std::lock_guard lock(m_CopyDataMutex);
        m_MemoryIndex = 0;
        m_RegionsToCopy.clear();
    }
    {
        std::lock_guard lock(m_DataMutex);

        m_AvailableBuffer++;
        m_AvailableBuffer %= VulkanRenderer::getFramesInFlight();

        m_PositionsToProcess.pop();
    }

    m_LastValidPosition = camPos;
    return true;
}

VkBufferImageCopy DynamicClipmapDeserializer::createRegion(const FileChunkProperties& task)
{
    VkBufferImageCopy region{};

    region.bufferOffset = m_MemoryIndex * m_TextureDataStride;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = task.Mip;
    region.imageSubresource.layerCount = 1;

    uint32_t x, y;
    unpackOffset(task.Position, x, y);
    uint32_t chunksPerRow = m_Specification.ClipmapSize / m_ChunkSize;

    x %= chunksPerRow;
    y %= chunksPerRow;


    int32_t chunkSizePadding = m_ChunkSize + 2;
    region.imageOffset = { int32_t(x * chunkSizePadding), int32_t(y * chunkSizePadding), 0 };
    region.imageExtent = { uint32_t(chunkSizePadding), uint32_t(chunkSizePadding), 1 };

    return region;
}
