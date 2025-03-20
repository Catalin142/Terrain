#pragma once

#include <cstdint>

struct GPUBruteForceRenderChunk
{
    uint32_t Offset;
    uint32_t Lod = 0;

    // uuuuuuuuddddddddllllllllrrrrrrrr
    uint32_t NeighboursLod = 0;
};