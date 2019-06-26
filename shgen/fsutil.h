#ifndef FSUTIL_H__
#define FSUTIL_H__
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fs
{
    struct FileMode
    {
        enum Type
        {
            Open  	= 0x0000,
            Create	= 0x0001,
            Append	= 0x0002,
        };
    };

    struct FileAccess
    {
        enum Type
        {
            Read    = 0x0010,
            Write   = 0x0020,
            RDRW    = 0x0030,
        };
    };

    struct FileShare
    {
        enum Type
        {
            None    = 0x0000,
            Read    = 0x0100,
            Write   = 0x0200,
            RDRW    = 0x0300,
        };
    };

    struct FileSeek
    {
        enum Type
        {
            Begin,
            Current,
            End,
        };
    };

    struct FileHandle
    {
        uint64_t id = UINT64_MAX;
        bool isInvalid() const { return id == UINT64_MAX; }
        operator uint64_t() const { return id; } 
    };

    struct FileInfo
    {
        std::string path;
        std::string abspath;
        size_t size;
        uint64_t mtime;
        uint64_t atime;
        uint64_t ctime;
    };

    FileHandle openFile(const std::string& path, uint32_t mode);
    void closeFile(FileHandle handle);
    size_t readFile(FileHandle handle, void* buf, size_t size);
    size_t writeFile(FileHandle handle, const void* buf, size_t size);
    size_t seekFile(FileHandle handle, int64_t offset, FileSeek::Type origin);
    size_t fileSize(FileHandle handle);

    void createDirectory(const std::string& path);

    std::string standardizePath(const std::string& path, bool appendLastSlash = false);
    void split(const std::string& path, std::string& dirname, std::string& basename);
    std::vector<FileInfo> findFiles(const std::string& pattern);
}

#endif
