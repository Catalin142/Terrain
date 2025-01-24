#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <vulkan/vulkan.h>

#include "Core/Hash.h"

#define SIZE_OF_FLOAT16 2

#define MAX_LOD 6u

struct GPUIndirectionNode
{
    uint32_t virtualLocation;
    uint32_t physicalLocation;
    uint32_t mip;
};

struct GPUVirtualMapProperties
{
    int32_t chunkSize;
    int32_t loadedNodesCount;
    int32_t padding[2];
};

struct GPUStatusNode
{
    uint32_t position;
    uint32_t mip;

    GPUStatusNode(uint32_t pos, uint32_t m, uint32_t status)
    {
        status &= 0x00000001;
        status <<= 16;
        m |= status;

        position = pos;
        mip = m;
    }
};

struct VirtualTerrainChunkProperties
{
    uint32_t Position;
    uint32_t Mip;
    size_t inFileOffset;
};

struct VirtualTextureLocation
{
    std::string Data;
    std::string Table;
};

struct VirtualTerrainMapSpecification
{
    uint32_t VirtualTextureSize = 1024;
    uint32_t ChunkSize = 128;

    uint32_t PhysicalTextureSize = 2048;
    VkFormat Format;

    VirtualTextureLocation Filepath;

    std::array<uint32_t, MAX_LOD> RingSizes;
};

struct LoadTask
{
    size_t Node;
    int32_t VirtualSlot;
    VirtualTerrainChunkProperties Properties;
};

struct VirtualMapDeserializerLastUpdate
{
    std::vector<GPUIndirectionNode> IndirectionNodes;
    std::vector<GPUStatusNode> StatusNodes;
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