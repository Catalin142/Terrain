#pragma once
#include <cstdint>

static size_t hash(uint32_t v1, uint32_t v2) {
	size_t h = (size_t(v1) << 32) + size_t(v2);
	h *= 1231231557ull;
	h ^= (h >> 32);
	return h;
}
