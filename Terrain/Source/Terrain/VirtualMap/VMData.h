#pragma once

#include <cstdint>

struct LoadedNode
{
    int32_t virtualLocation;
    int32_t physicalLocation;
    int32_t mip;
    int32_t padding;
};

struct VirtualMapProperties
{
    int chunkSize;
    int loadedNodesCount;
    int padding[2];
};
