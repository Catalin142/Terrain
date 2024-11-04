#pragma once
#include "Core/Hash.h"

#define SIZE_OF_FLOAT16 2

static uint32_t packOffset(uint32_t x, uint32_t y)
{
    uint32_t xOffset = x & 0x0000ffff;
    uint32_t yOffset = (y & 0x0000ffff) << 16;
    return xOffset | yOffset;
}

static void unpackOffset(uint32_t offset, uint32_t& x, uint32_t& y)
{
    x = offset & 0x0000ffff;
    y = (offset & 0xffff0000) >> 16;
}

static size_t getChunkID(uint32_t offsetX, uint32_t offsetY, uint32_t mip)
{
    return hash(packOffset(offsetX, offsetY), mip);
}

static size_t getChunkID(uint32_t offset, uint32_t mip)
{
    return hash(offset, mip);
}