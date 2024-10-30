#pragma once

#include <cstdint>

struct LoadedNode
{
    int virtualLocation;
    int physicalLocation;
    int mip;
    int padding;
};

struct VirtualMapProperties
{
    int chunkSize;
    int loadedNodesCount;
    int padding[2];
};
