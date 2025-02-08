#pragma once

#include <string>
#include <zstd.h>

// Dictionary compressor wrapper around ZSTD
class DCompressor
{
public:
	DCompressor(const std::string& dictionaryFilepath, int32_t cLevel);
	~DCompressor();

	size_t Compress(char* dst, size_t dstSize, char* src, size_t srcSize);

private:
	ZSTD_CCtx* m_CompressionContext;
	ZSTD_CDict* m_CompressionDictionary;
};

class DDecompressor
{
public:
	DDecompressor(const std::string& dictionaryFilepath);
	~DDecompressor();

	size_t Decompress(char* dst, size_t dstSize, char* src, size_t srcSize);

private:
	ZSTD_DCtx* m_DecompressionContext;
	ZSTD_DDict* m_DecompressionDictionary;
};