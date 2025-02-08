#include "DynamicClipmapDeserializer.h"

#include "Terrain/TerrainChunk.h"
#include "Core/DCompressor.h"

DynamicClipmapDeserializer::DynamicClipmapDeserializer(const ClipmapSpecification& spec) : m_Specification(spec)
{
    m_TextureDataStride = m_Specification.ChunkSize * m_Specification.ChunkSize * 2;

    {
        VulkanBufferProperties RawDataProperties;
        RawDataProperties.Size = m_TextureDataStride * MAX_CHUNKS_LOADING_PER_FRAME;
        RawDataProperties.Type = BufferType::TRANSFER_SRC_BUFFER | BufferType::TRANSFER_DST_BUFFER;
        RawDataProperties.Usage = BufferMemoryUsage::BUFFER_CPU_VISIBLE | BufferMemoryUsage::BUFFER_CPU_COHERENT;

        m_RawImageData = std::make_shared<VulkanBuffer>(RawDataProperties);

        m_RawImageData->Map();
    }

    m_RegionsToCopy.reserve(MAX_CHUNKS_LOADING_PER_FRAME);
}

static char* compressedBuffer = new char[128 * 128 * 2];
static DDecompressor* decompressor = new DDecompressor("Resources/Compression/heightmapDictionary");

void DynamicClipmapDeserializer::loadChunk(const FileChunkProperties& task)
{
    if (m_FileHandle == nullptr)
        m_FileHandle = new std::ifstream(m_Specification.Filepath.Data, std::ios::binary);

    if (!m_FileHandle->is_open())
        throw(false);

    assert(m_MemoryIndex != MAX_CHUNKS_LOADING_PER_FRAME);

    m_FileHandle->seekg(task.inFileOffset, std::ios::beg);

    char* charData = (char*)m_RawImageData->getMappedData();
    
    m_FileHandle->read(compressedBuffer, task.Size);
    decompressor->Decompress(&charData[m_MemoryIndex * m_TextureDataStride], m_TextureDataStride, compressedBuffer, task.Size);

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
    uint32_t chunksPerRow = m_Specification.TextureSize / m_Specification.ChunkSize;

    x %= chunksPerRow;
    y %= chunksPerRow;

    region.imageOffset = { int32_t(x * m_Specification.ChunkSize), int32_t(y * m_Specification.ChunkSize), 0 };
    region.imageExtent = { m_Specification.ChunkSize, m_Specification.ChunkSize, 1 };

    m_RegionsToCopy.push_back(region);

    m_MemoryIndex++;
}

void DynamicClipmapDeserializer::Flush()
{
    m_RegionsToCopy.clear();
    m_MemoryIndex = 0;
}
