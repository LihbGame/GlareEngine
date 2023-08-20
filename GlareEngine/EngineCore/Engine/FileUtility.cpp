#include "FileUtility.h"
#include <fstream>
#include <mutex>
#include <zlib.h>
#if _HAS_CXX17
#include <filesystem>
#endif
#include "EngineLog.h"

using namespace std;
using namespace GlareEngine;

ByteArray GlareEngine::NullFile = make_shared<vector<byte> >(vector<byte>());

ByteArray DecompressZippedFile(wstring& fileName);

ByteArray ReadFileHelper(const wstring& fileName)
{
    struct _stat64 fileStat;
    int fileExists = _wstat64(fileName.c_str(), &fileStat);
    if (fileExists == -1)
        return NullFile;

    ifstream file(fileName, ios::in | ios::binary);
    if (!file)
        return NullFile;

   ByteArray byteArray = make_shared<vector<byte> >(fileStat.st_size);
    file.read((char*)byteArray->data(), byteArray->size());
    file.close();

    return byteArray;
}


ByteArray ReadFileHelperEx(shared_ptr<wstring> fileName)
{
    std::wstring zippedFileName = *fileName + L".gz";
    ByteArray firstTry = DecompressZippedFile(zippedFileName);
    if (firstTry != NullFile)
        return firstTry;

    return ReadFileHelper(*fileName);
}

ByteArray Inflate(ByteArray CompressedSource, int& err, uint32_t ChunkSize = 0x100000)
{
    // Create a dynamic buffer to hold compressed blocks
    vector<unique_ptr<byte> > blocks;

    z_stream strm = {};
    strm.data_type = Z_BINARY;
    strm.total_in = strm.avail_in = (uInt)CompressedSource->size();
    strm.next_in = CompressedSource->data();

    err = inflateInit2(&strm, (15 + 32)); //15 window bits, and the +32 tells zlib to to detect if using gzip or zlib

    while (err == Z_OK || err == Z_BUF_ERROR)
    {
        strm.avail_out = ChunkSize;
        strm.next_out = (byte*)malloc(ChunkSize);
        blocks.emplace_back(strm.next_out);
        err = inflate(&strm, Z_NO_FLUSH);
    }

    if (err != Z_STREAM_END)
    {
        inflateEnd(&strm);
        return NullFile;
    }

    assert(strm.total_out > 0);

    ByteArray byteArray = make_shared<vector<byte> >(strm.total_out);

    // Allocate actual memory for this.
    // copy the bits into that RAM.
    // Free everything else up!!
    void* curDest = byteArray->data();
    size_t remaining = byteArray->size();

    for (size_t i = 0; i < blocks.size(); ++i)
    {
        assert(remaining > 0);

        size_t CopySize = remaining < ChunkSize ? remaining : ChunkSize;

        memcpy(curDest, blocks[i].get(), CopySize);
        curDest = (byte*)curDest + CopySize;
        remaining -= CopySize;
    }

    inflateEnd(&strm);

    return byteArray;
}

ByteArray DecompressZippedFile(wstring& fileName)
{
    ByteArray CompressedFile = ReadFileHelper(fileName);
    if (CompressedFile == NullFile)
        return NullFile;

    int error;
    ByteArray DecompressedFile = Inflate(CompressedFile, error);
    if (DecompressedFile->size() == 0)
    {
        EngineLog::AddLog(L"Couldn't unzip file %s:  Error = %d\n", fileName.c_str(), error);
        return NullFile;
    }
    return DecompressedFile;
}

ByteArray FileUtility::ReadFileSync(const wstring& fileName)
{
    return ReadFileHelperEx(make_shared<wstring>(fileName));
}

task<ByteArray> FileUtility::ReadFileAsync(const wstring& fileName)
{
    shared_ptr<wstring> SharedPtr = make_shared<wstring>(fileName);
    return create_task([=] { return ReadFileHelperEx(SharedPtr); });
}

bool FileUtility::IsFileExists(const char* filePath)
{
    assert(filePath);
#if _HAS_CXX17
    return std::filesystem::exists(filePath);
#else
#if _WIN32
    if (auto fileHandle = ::CreateFileA(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY,
        nullptr))
    {
        ::CloseHandle(fileHandle);
        return true;
    }
    return false;
#else
    std::ifstream stream(filePath, std::ios::in);
    bool exists = stream.good();
    stream.close();
    return exists;
#endif
#endif
}

size_t FileUtility::GetFileSize(const char* filePath)
{
    assert(filePath);
#if _HAS_CXX17
    return std::filesystem::file_size(filePath);
#else
#if _WIN32
    size_t size = 0u;
    if (auto fileHandle = ::CreateFileA(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY,
        nullptr))
    {
        ::LARGE_INTEGER fileSize{};
        if (::GetFileSizeEx(fileHandle, &fileSize))
        {
            size = fileSize.HighPart;
        }
        ::CloseHandle(fileHandle);
    }
    return size;
#else
    size_t size = 0u;
    std::ifstream stream(filePath, std::ios::in | std::ios::ate);
    if (stream.good())
    {
        size = static_cast<size_t>(stream.tellg());
    }
    stream.close();
    return size;
#endif
#endif
}

std::time_t FileUtility::GetFileLastWriteTime(const char* filePath)
{
    assert(filePath);
#if _HAS_CXX17
    if (std::filesystem::exists(filePath))
    {
        return std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
            std::filesystem::last_write_time(filePath).time_since_epoch()).count();
    }
#else
#if _WIN32
    std::time_t time = 0;
    if (auto fileHandle = ::CreateFileA(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY,
        nullptr))
    {
        ::FILETIME lastWriteTime{};
        if (::GetFileTime(fileHandle, nullptr, nullptr, &lastWriteTime))
        {
            time = *reinterpret_cast<std::time_t*>(&lastWriteTime);
        }
        ::CloseHandle(fileHandle);
    }
    return time;
#else
    assert(false);
#endif
#endif
}

ByteArray FileUtility::ReadFile(const char* filePath, EFileMode mode)
{
    auto fileSize = GetFileSize(filePath);
    ByteArray byteArray = NullFile;
    if (fileSize > 0u)
    {
        ifstream stream(filePath, mode == EFileMode::Binary ? (std::ios::in | std::ios::binary) : std::ios::in);
        if (stream.good())
        {
            byteArray = make_shared<vector<byte>>(fileSize);
            stream.read((char*)byteArray->data(), fileSize);
        }
        stream.close();
    }

    return byteArray;
}
