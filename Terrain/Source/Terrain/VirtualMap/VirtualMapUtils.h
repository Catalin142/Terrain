#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <vulkan/vulkan.h>

#include "Core/Hash.h"
#include "Terrain/TerrainChunk.h"

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

struct VirtualTerrainMapSpecification
{
    uint32_t VirtualTextureSize = 1024;
    uint32_t ChunkSize = 128;

    uint32_t PhysicalTextureSize = 2048;
    VkFormat Format;

    TerrainFileLocation Filepath;

    std::array<uint32_t, MAX_LOD> RingSizes;
};

struct VirtualMapLoadTask
{
    size_t Node;
    int32_t VirtualSlot;
    FileChunkProperties Properties;
};

struct VirtualMapDeserializerLastUpdate
{
    std::vector<GPUIndirectionNode> IndirectionNodes;
    std::vector<GPUStatusNode> StatusNodes;
};