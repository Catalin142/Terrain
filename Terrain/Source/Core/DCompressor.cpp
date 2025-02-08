#include "DCompressor.h"

#include <fstream>

DCompressor::DCompressor(const std::string& dictionaryFilepath, int32_t cLevel)
{
    std::ifstream dictionaryFile(dictionaryFilepath, std::ios::binary);

    dictionaryFile.seekg(0, dictionaryFile.end);
    size_t dictSize = dictionaryFile.tellg();
    dictionaryFile.seekg(0, dictionaryFile.beg);

    void* const dictBuffer = (void*)new char[dictSize];
    dictionaryFile.read((char*)dictBuffer, dictSize);
    dictionaryFile.close();

    m_CompressionDictionary = ZSTD_createCDict(dictBuffer, dictSize, cLevel);
    m_CompressionContext = ZSTD_createCCtx();

    delete[] dictBuffer;
}

DCompressor::~DCompressor()
{
    ZSTD_freeCDict(m_CompressionDictionary);
    ZSTD_freeCCtx(m_CompressionContext);
}

size_t DCompressor::Compress(char* dst, size_t dstSize, char* src, size_t srcSize)
{
	return ZSTD_compress_usingCDict(m_CompressionContext, dst, dstSize, src, srcSize, m_CompressionDictionary);
}

DDecompressor::DDecompressor(const std::string& dictionaryFilepath)
{
    std::ifstream dictionaryFile(dictionaryFilepath, std::ios::binary);

    dictionaryFile.seekg(0, dictionaryFile.end);
    size_t dictSize = dictionaryFile.tellg();
    dictionaryFile.seekg(0, dictionaryFile.beg);

    void* const dictBuffer = (void*)new char[dictSize];
    dictionaryFile.read((char*)dictBuffer, dictSize);
    dictionaryFile.close();

    m_DecompressionDictionary = ZSTD_createDDict(dictBuffer, dictSize);
    m_DecompressionContext = ZSTD_createDCtx();

    delete[] dictBuffer;
}

DDecompressor::~DDecompressor()
{
    ZSTD_freeDDict(m_DecompressionDictionary);
    ZSTD_freeDCtx(m_DecompressionContext);
}

size_t DDecompressor::Decompress(char* dst, size_t dstSize, char* src, size_t srcSize)
{
	return ZSTD_decompress_usingDDict(m_DecompressionContext, dst, dstSize, src, srcSize, m_DecompressionDictionary);
}
