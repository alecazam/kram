// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramFileHelper.h"

// here's how to mmmap data, but NSData may have another way
#include <stdio.h>
#include <sys/stat.h>

#include <errno.h>

// Use this for consistent tmp file handling
//#include <algorithm> // for min
//#include <vector>

#include "tmpfileplus.h"

#if KRAM_MAC || KRAM_IOS || KRAM_LINUX
#include <unistd.h>  // for getpagesize()
#endif

#if KRAM_WIN
#include <windows.h>  // for GetNativeSystemInfo()
#include <direct.h> // direct-ory for _mkdir, _rmdir

// Windows mkdir doesn't take permission
#define mkdir(fname, permission) _mkdir(fname)
#define rmdir(fname) _rmdir(fname)
#endif

namespace kram {
using namespace NAMESPACE_STL;

#define nl "\n"

// https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c
static void mkdirRecursive(char *path) {
    char *sep = strrchr(path, '/');
    if (sep != NULL) {
        *sep = 0;
        mkdirRecursive(path);
        *sep = '/';
    }
    
    if (*path != '\0' && mkdir(path, 0755) && errno != EEXIST)
    {
        KLOGE("kram", "error while trying to create '%s'" nl
               "%s" nl,
               path, strerror(errno)); // same as %m
    }
}

static FILE *fopen_mkdir(const char *path, const char *mode) {
    const char *sep = strrchr(path, '/');
    if(sep) {
        char *path0 = strdup(path);
        path0[ sep - path ] = 0;
        mkdirRecursive(path0);
        free(path0);
    }
    
    return fopen(path,mode);
}

FileHelper::~FileHelper() { close(); }

// no current extension
bool FileHelper::openTemporaryFile(const char* suffix, const char* access)
{
    close();

    // temp file api in POSIX is way too convoluted
    // might want to update to std::filesystem that has possibly abstracted this
    // it's likely the C code doesn't translate to Windows, and std::filesystem
    // has a move operation not found in C calls that can also create intermediate dirs.

    (void)access;

    char* pathname = nullptr;

    // can't rename with this set to 0, but it will autodelete tmp file
    int keep = 0;

    // Note: can't pass . either, always opened as rw
    _fp = tmpfileplus("/tmp/", "kramimage-", suffix, &pathname, keep);
    if (!_fp) {
        return false;
    }

    _isTmpFile = true;
    _filename = pathname;
    free(pathname);

    return true;
}

bool FileHelper::read(uint8_t* data, size_t dataSize)
{
    return FileHelper::readBytes(_fp, data, dataSize);
}
bool FileHelper::write(const uint8_t* data, size_t dataSize)
{
    return FileHelper::writeBytes(_fp, data, dataSize);
}

bool FileHelper::readBytes(FILE* fp, uint8_t* data, size_t dataSize)
{
    size_t elementsRead = fread(data, 1, dataSize, fp);
    if (elementsRead != (size_t)dataSize) {
        return false;
    }
    return true;
}
bool FileHelper::writeBytes(FILE* fp, const uint8_t* data, size_t dataSize)
{
    size_t elementsWritten = fwrite(data, 1, dataSize, fp);
    if (elementsWritten != (size_t)dataSize) {
        return false;
    }
    return true;
}

size_t FileHelper::pagesize()
{
    static size_t pagesize = 0;
    if (pagesize == 0) {
#if KRAM_MAC || KRAM_IOS || KRAM_LINUX
        pagesize = getpagesize();
#elif KRAM_WIN
        SYSTEM_INFO systemInfo;
        GetNativeSystemInfo(&systemInfo);
        pagesize = systemInfo.dwPageSize;
#else
        pagesize = 4 * 1024;  // how to determine on Win/Linux?
#endif
    }
    return pagesize;
}

bool FileHelper::copyTemporaryFileTo(const char* dstFilename)
{
    if (!_fp) return false;
    if (_filename.empty()) return false;

    // since we're not closing, need to flush output
    fflush(_fp);

    size_t size_ = size();
    if (size_ == (size_t)-1) {
        return false;
    }
    
    // DONE: copy in smaller buffered chunks
    size_t maxBufferSize = 256*1024;
    size_t bufferSize = min(size_, maxBufferSize);
    vector<uint8_t> tmpBuf;
    tmpBuf.resize(bufferSize);

    // rewind to the beginning
    rewind(_fp);

    // Note: was trying to use rename and moveTemporaryFileTo to save a copy
    // but it needs a filename and unlink breaks that for auto-delete of temp file
    // and rename behaves completely differently on Win vs. Mac despite being stdio - ugh.
    // and rename breaks on Mac copying cross volume when tmp/file live on different volumes

    // need to fopen file on potentially other volume,
    // then buffer copy the contents over to the over drive
    // temp file will automatically be destroyed with in close() call
    // but can copy it as many times as needed.

    FileHelper dstHelper;
    if (!dstHelper.open(dstFilename, "w+b")) {
        return false;
    }
    
    size_t bytesRemaining = size_;
    while(bytesRemaining > 0) {
        int bytesToRead = min(bufferSize, bytesRemaining);
        bytesRemaining -= bytesToRead;
        
        if (!read(tmpBuf.data(), bytesToRead) ||
            !dstHelper.write(tmpBuf.data(), bytesToRead)) {
            dstHelper.close();

            // don't leave a partially written file
            remove(dstFilename);
            return false;
        }
    }
    
    return true;
}

bool FileHelper::open(const char* filename, const char* access)
{
    close();

    if (strstr(access, "w") != nullptr) {
        _fp = fopen_mkdir(filename, access);
    }
    else {
        _fp = fopen(filename, access);
    }
    
    if (!_fp) {
        return false;
    }

    _filename = filename;
    return true;
}

void FileHelper::close()
{
    if (!_fp) {
        return;
    }

    // temp files are auto-deleted on fclose, since they've been "keep" is 0
    fclose(_fp);

    _isTmpFile = false;
    _fp = nullptr;
}

size_t FileHelper::size() const
{
    // returns -1, so can't return size_t
    if (!_fp) {
        return (size_t)-1;
    }

    // otherwise fstat won't extract the size
    fflush(_fp);

    int fd = fileno(_fp);

    struct stat stats;
    if (fstat(fd, &stats) < 0) {
        return (size_t)-1;
    }
    return (int64_t)stats.st_size;
}

bool FileHelper::exists(const char* filename) const
{
    struct stat stats;
    if (stat(filename, &stats) < 0) {
        return false;
    }
    return true;
}

uint64_t FileHelper::modificationTimestamp(const char* filename) {
  
    // Win has to rename all this, so make it happy using wrappers from miniz
    #if defined(_MSC_VER) || defined(__MINGW64__)
    #define MZ_FILE_STAT_STRUCT _stat64
    #define MZ_FILE_STAT _stat64
    #else
    #define MZ_FILE_STAT_STRUCT stat
    #define MZ_FILE_STAT stat
    #endif

    struct MZ_FILE_STAT_STRUCT stats;
    if (MZ_FILE_STAT(filename, &stats) < 0) {
        return 0;
    }
       
    // https://www.quora.com/What-is-the-difference-between-mtime-atime-and-ctime
    // atime is last access time
    // ctime when attributes change
    // mtime when contents change
    // folders mtime changes when files added/deleted
    
    // 32.32, only return seconds for now
    // https://stackoverflow.com/questions/11373505/getting-the-last-modified-date-of-a-file-in-c
    return stats.st_mtime;
}


}  // namespace kram
