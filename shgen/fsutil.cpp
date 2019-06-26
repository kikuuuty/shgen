#include "fsutil.h"

#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define MAKE_QWORD(low,high) (uint64_t)(low|((uint64_t)high<<32))

namespace
{
    std::vector<std::string> split(const std::string& str, const std::string& sep = " ", int maxSplits = -1)
    {
        std::vector<std::string> result;
        result.reserve(maxSplits ? maxSplits+1 : 8);

        int numSplits = 0;

        size_t start = 0, pos;
        do {
            pos = str.find_first_of(sep, start);
            if (pos == start) {
                // 検索開始位置にデリミタが見つかった。
                start = pos + 1;
                result.push_back(std::string());
            }
            else if (pos == std::string::npos || (maxSplits >= 0 && numSplits == maxSplits)) {
                // 残りを追加する。
                result.push_back(str.substr(start));
                break;
            }
            else {
                // 区切られた範囲を追加する。
                result.push_back(str.substr(start, pos - start));
                start = pos + 1;
            }

            //start = str.find_first_not_of(sep, start);
            ++numSplits;

        } while (pos != std::string::npos);
        return result;
    }

    void splitWithWildcard(const std::string& path, std::string& dirname, std::string& pattern)
    {
        auto tmp = path;
        std::replace(tmp.begin(), tmp.end(), '\\', '/');
        auto w = tmp.find_first_of('*');
        auto i = tmp.find_last_of('/');
        if (w != std::string::npos && w < i) {
            i = tmp.substr(0, w).find_last_of('/');
        }

        if (i != std::string::npos) {
            dirname = tmp.substr(0, i);
            pattern = tmp.substr(i + 1);
        }
        else {
            dirname.clear();
            pattern = std::move(path);
        }
    }

    bool match(const std::string& str, const std::string& pattern)
    {
        std::string tmpStr = str;
        std::string tmpPattern = pattern;

        auto strIt = tmpStr.begin();
        auto patIt = tmpPattern.begin();
        auto lastWildcardIt = tmpPattern.end();
        while (strIt != tmpStr.end() && patIt != tmpPattern.end()) {
            if (*patIt == '*') {
                lastWildcardIt = patIt;
                // ワイルドカードの次へ..
                ++patIt;
                if (patIt == tmpPattern.end()) {
                    // 末尾まで飛ばす。
                    strIt = tmpStr.end();
                }
                else {
                    // ワイルドカードの次の文字と一致するまで走査する。
                    while (strIt != tmpStr.end() && *strIt != *patIt) {
                        ++strIt;
                    }
                }
            }
            else {
                if (*patIt != *strIt) {
                    if (lastWildcardIt != tmpPattern.end()) {
                        // ワイルドカードの次の文字と文字列内の文字が一致している場合、
                        // その文字はまだワイルドカードの範囲内の可能性があるので、
                        // ワイルドカードのパターンを巻き戻して検索を続ける。
                        patIt = lastWildcardIt;
                        lastWildcardIt = tmpPattern.end();
                    }
                    else {
                        return false;
                    }
                }
                else {
                    // 文字は一致している。
                    ++patIt;
                    ++strIt;
                }
            }
        }
        // 文字列もパターンも最後まで走査されていたら完全に一致。
        return (patIt == tmpPattern.end() && strIt == tmpStr.end()) ? true : false;
    }

    std::wstring utf8ToUtf16(const std::string& u8str)
    {
        int u16strLen = ::MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, NULL, 0);
        if (u16strLen <= 0) return std::wstring();
        std::wstring u16str;
        u16str.resize(u16strLen-1);
        ::MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, &u16str[0], u16strLen);
        return u16str;
    }

    std::string utf16ToUtf8(const std::wstring& u16str)
    {
        int u8strLen = ::WideCharToMultiByte(CP_UTF8, 0, u16str.c_str(), -1, NULL, 0, NULL, NULL);
        if (u8strLen <= 0) return std::string();
        std::string u8str;
        u8str.resize(u8strLen - 1);
        ::WideCharToMultiByte(CP_UTF8, 0, u16str.c_str(), -1, &u8str[0], u8strLen, NULL, NULL);
        return u8str;
    }

    std::string getcwd()
    {
        WCHAR path[MAX_PATH];
        DWORD ret = ::GetCurrentDirectoryW(MAX_PATH, path);
        return ret == 0 ? std::string() : fs::standardizePath(utf16ToUtf8(path), true);
    }

    inline bool isReservedDir(const WCHAR* path)
    {
        return (path[0] == '.' && (path[1] == '\0' || (path[1] == '.' && path[2] == '\0')));
    }

    void findFilesRecursively(std::vector<fs::FileInfo>& v, const std::string& dirpath)
    {
        WIN32_FIND_DATAW fd;
        HANDLE handle = ::FindFirstFileW(utf8ToUtf16(dirpath + '*').c_str(), &fd);
        if (handle == INVALID_HANDLE_VALUE)
            return;

        for (BOOL res = TRUE; res == TRUE; res = ::FindNextFileW(handle, &fd)) {
            DWORD attrs = fd.dwFileAttributes;
            if (attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
                continue;
            }
            else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                if (isReservedDir(fd.cFileName))
                    continue;
                findFilesRecursively(v, dirpath + utf16ToUtf8(fd.cFileName) + '/');
            }
            else if (attrs & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) {
                std::string filename = dirpath + utf16ToUtf8(fd.cFileName);

                uint64_t fsize = MAKE_QWORD(fd.nFileSizeLow, fd.nFileSizeHigh);
                uint64_t mtime = MAKE_QWORD(fd.ftLastWriteTime.dwLowDateTime, fd.ftLastWriteTime.dwHighDateTime);
                uint64_t atime = MAKE_QWORD(fd.ftLastAccessTime.dwLowDateTime, fd.ftLastAccessTime.dwHighDateTime);
                uint64_t ctime = MAKE_QWORD(fd.ftCreationTime.dwLowDateTime, fd.ftCreationTime.dwHighDateTime);

                fs::FileInfo info;
                info.path = filename;
                info.abspath = getcwd() + filename;
                info.size = static_cast<size_t>(fsize);
                info.mtime = mtime / 10000; // FILETIMEは100nsec単位。
                info.atime = atime / 10000;
                info.ctime = ctime / 10000;
                v.push_back(std::move(info));
            }
        }
        ::FindClose(handle);
    }
}

namespace fs
{
    FileHandle openFile(const std::string& path, uint32_t mode)
    {
        DWORD desiredAccess = 0;
        if (mode & FileAccess::Read)  desiredAccess |= GENERIC_READ;
        if (mode & FileAccess::Write) desiredAccess |= GENERIC_WRITE;

        DWORD shareMode = FILE_SHARE_READ;
        if (mode & FileShare::Read)  shareMode |= FILE_SHARE_READ;
        if (mode & FileShare::Write) shareMode |= FILE_SHARE_WRITE;

        const uint32_t IO_MODE_MASK = 3;

        DWORD creationDisposition = 0;
        if ((mode & IO_MODE_MASK) == FileMode::Open)   creationDisposition = OPEN_EXISTING;
        if ((mode & IO_MODE_MASK) == FileMode::Create) creationDisposition = CREATE_ALWAYS;
        if ((mode & IO_MODE_MASK) == FileMode::Append) creationDisposition = OPEN_ALWAYS;

        DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        std::wstring filename = utf8ToUtf16(path);
        HANDLE handle = ::CreateFile(filename.c_str(), desiredAccess, shareMode, NULL, creationDisposition,
                                     flagsAndAttributes, NULL);
        if (handle == INVALID_HANDLE_VALUE) return FileHandle();
        return FileHandle{uint64_t(handle)};
    }

    void closeFile(FileHandle handle)
    {
        if (handle.isInvalid()) return;
        ::CloseHandle(HANDLE(handle.id));
    }

    size_t readFile(FileHandle handle, void* buf, size_t size)
    {
        if (handle.isInvalid()) return 0;

        int64_t readSize = 0;
        int64_t remain = (int64_t)std::min(size, uint64_t(INT64_MAX));
        while (remain > 0) {
            DWORD bytesToRead = (DWORD)std::min(remain, int64_t(UINT_MAX));
            DWORD readBytes;
            if (0 == ::ReadFile(HANDLE(handle.id), buf, bytesToRead, &readBytes, NULL)) {
                return 0;
            }
            remain -= readBytes;
            readSize += readBytes;
            buf = (void*)(uintptr_t(buf) + readBytes);
        }
        return readSize;
    }

    size_t writeFile(FileHandle handle, const void* buf, size_t size)
    {
        if (handle.isInvalid()) return 0;

        int64_t writtenSize = 0;
        int64_t remain = (int64_t)std::min(size, uint64_t(INT64_MAX));
        while (remain > 0) {
            DWORD bytesToWrite = (DWORD)std::min(remain, int64_t(UINT_MAX));
            DWORD writtenBytes;
            if (0 == ::WriteFile(HANDLE(handle.id), buf, bytesToWrite, &writtenBytes, NULL)) {
                DWORD err = ::GetLastError();
                (void)err;
                return 0;
            }
            remain -= writtenBytes;
            writtenSize += writtenBytes;
            buf = (void*)(uintptr_t(buf) + writtenBytes);
        }
        return writtenSize;
    }

    size_t seekFile(FileHandle handle, int64_t offset, FileSeek::Type origin)
    {
        if (handle.isInvalid()) return 0;

        LARGE_INTEGER distanceToMove;
        LARGE_INTEGER newFilePtr;
        distanceToMove.QuadPart = offset;
        if (0 == ::SetFilePointerEx(HANDLE(handle.id), distanceToMove, &newFilePtr, static_cast<DWORD>(origin))) {
            return 0;
        }
        return size_t(newFilePtr.QuadPart);
    }

    size_t fileSize(FileHandle handle)
    {
        if (handle.isInvalid()) return 0;

        LARGE_INTEGER size;
        if (0 == ::GetFileSizeEx(HANDLE(handle.id), &size)) {
            return 0;
        }
        return size_t(size.QuadPart);
    }

    void createDirectory(const std::string& path)
    {
        std::vector<std::string> dirs = ::split(path, "/");
        std::string fullpath;
        for (auto& dir : dirs) {
            fullpath += dir + '/';
            ::CreateDirectoryW(utf8ToUtf16(fullpath).c_str(), NULL);
        }
    }

    std::string standardizePath(const std::string& path, bool appendLastSlash)
    {
        std::string tmp = path;
        if (!tmp.empty()) {
            std::replace(tmp.begin(), tmp.end(), '\\', '/');
            if (appendLastSlash && tmp.back() != '/') tmp.append(1, '/');
        }
        return tmp;
    }

    void split(const std::string& path, std::string& dirname, std::string& basename)
    {
        auto tmp = path;
        std::replace(tmp.begin(), tmp.end(), '\\', '/');
        auto i = tmp.find_last_of('/');
        if (i != std::string::npos) {
            dirname = tmp.substr(0, i);
            basename = tmp.substr(i + 1);
        }
        else {
            dirname.clear();
            basename = std::move(path);
        }
    }

    std::vector<FileInfo> findFiles(const std::string& pattern)
    {
        std::vector<FileInfo> v;

        std::string dirname, filename;
        splitWithWildcard(pattern, dirname, filename);

        dirname = standardizePath(dirname, true);
        findFilesRecursively(v, dirname);

        if (filename != "*") {
            std::string filter = dirname + filename;
            auto it = std::remove_if(v.begin(), v.end(), [&filter] (const FileInfo& f) { return !match(f.path, filter); });
            v.erase(it, v.end());
        }
        return v;
    }
}
