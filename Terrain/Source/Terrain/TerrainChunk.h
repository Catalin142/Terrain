#pragma once

#include <vector>
#include <string>
#include <memory>

#include <glm/glm.hpp>

#include "Core/Hash.h"
#include "Graphics/Vulkan/VulkanBuffer.h"

#define SIZE_OF_FLOAT16 2

struct TerrainChunk
{
	// x 16 bits, y 16 bits
	uint32_t Offset;
	uint32_t Lod = 0;

	static std::vector<uint32_t> generateIndices(uint32_t lod, uint32_t vertCount);
	static void generateIndicesAndVertices(uint32_t vertCount, std::vector<uint32_t>& indices, std::vector<glm::ivec2>& vertices);
};

struct TerrainChunkIndexBuffer
{
    std::shared_ptr<VulkanBuffer> IndexBuffer;
    uint32_t IndicesCount = 0;

    ~TerrainChunkIndexBuffer()
    {
        int r = 5;
    }
};

struct FileChunkProperties
{
    uint32_t Position;
    uint32_t Mip;
    size_t inFileOffset;
    size_t Size;
};

struct TerrainFileLocation
{
    std::string Data;
    std::string Table;
};

static uint32_t packOffset(uint32_t x, uint32_t y)
{
    uint32_t xOffset = x & 0x0000ffff;
    uint32_t yOffset = (y & 0x0000ffff) << 16;
    return xOffset | yOffset;
}

static void unpackOffset(uint32_t offset, uint32_t& x, uint32_t& y)
{
    x = offset & 0x0000ffff;
    y = (offset >> 16) & 0x0000ffff;
}

static size_t getChunkID(uint32_t offsetX, uint32_t offsetY, uint32_t mip)
{
    return hash(packOffset(offsetX, offsetY), mip);
}

static size_t getChunkID(uint32_t offset, uint32_t mip)
{
    return hash(offset, mip);
}
