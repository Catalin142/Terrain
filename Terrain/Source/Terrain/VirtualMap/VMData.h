#pragma once

#include <cstdint>

struct LoadedNode
{
    uint32_t virtualLocation;
    uint32_t physicalLocation;
    uint32_t mip;
    uint32_t padding;
};

struct VirtualMapProperties
{
    int chunkSize;
    int loadedNodesCount;
    int padding[2];
};
