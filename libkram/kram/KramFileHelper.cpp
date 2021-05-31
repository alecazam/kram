// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramFileHelper.h"

// here's how to mmmap data, but NSData may have another way
#include <stdio.h>
#include <sys/stat.h>

// Use this for consistent tmp file handling
#include <algorithm> // for min
#include <vector>

#include "tmpfileplus/tmpfileplus.h"

#if KRAM_MAC || KRAM_IOS || KRAM_LINUX
#include <unistd.h>  // for getpagesize()
#endif
#if KRAM_WIN
#include <windows.h>  // for GetNativeSystemInfo()
#endif

namespace kram {
using namespace std;

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

    // Note: can't pass access either, always opened as rw
    _fp = tmpfileplus("/tmp/", "kramimage-", suffix, &pathname, keep);
    if (!_fp) {
        return false;
    }

    _isTmpFile = true;
    _filename = pathname;
    free(pathname);

    return true;
}

bool FileHelper::read(uint8_t* data, int dataSize)
{
    return FileHelper::readBytes(_fp, data, dataSize);
}
bool FileHelper::write(const uint8_t* data, int dataSize)
{
    return FileHelper::writeBytes(_fp, data, dataSize);
}

bool FileHelper::readBytes(FILE* fp, uint8_t* data, int dataSize)
{
    size_t elementsRead = fread(data, 1, dataSize, fp);
    if (elementsRead != (size_t)dataSize) {
        return false;
    }
    return true;
}
bool FileHelper::writeBytes(FILE* fp, const uint8_t* data, int dataSize)
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

    int size_ = size();
    if (size_ < 0) {
        return false;
    }
    
    // DONE: copy in smaller buffered chunks
    int maxBufferSize = 256*1024;
    int bufferSize = min(size_, maxBufferSize);
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
    
    int bytesRemaining = size_;
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

/* This code was original attempt to move file, but it interfered with unlink of the file
    since a closed file was needed for rename() and many many other issues.
 
bool FileHelper::moveTemporaryFileTo(const char* dstFilename)
{
    if (!_fp) return false;
    if (_filename.empty()) return false;

#if USE_TMPFILEPLUS
    fclose(_fp);

    // windows doesn't remove any existing file, so have to do it explicitly
    //remove(dstFilename);
    //
    // now do the rename
    // rename on Windows does a copy if different volumes, but no way to identify if it did or moved the file
    // so tmp file would need to be auto deleted then.  Could call MoveFileEx twice, with and without COPY
    // if the move failed.
    //bool success = (rename(_filename.c_str(), dstFilename) == 0);

    // this is probably better than remove/rename, and maybe works across volumes/partitions
    // this can't replace directories and will fail, only for files
    bool success = MoveFileEx(_filename.c_str(), dstFilename, MOVEFILE_REPLACE_EXISTING) == 0;
    if (!success) {
        MoveFileEx(_filename.c_str(), dstFilename, MOVEFILE_COPY_ALLOWED);
        
        // since move was expected, delete the source, it's had fclose already called
        remove(_filename.c_str());
    }
    
    // so that close doesn't do another fclose()
    _fp = nullptr;
    _isTmpFile = false;
    _filename.clear();

#else
    // since we're not closing, need to flush output
    fflush(_fp);

    // somehow even though the file isn't closed, can rename it
    // if an open temp file is closed, then the following fails since the fd/fp are released.

    // rename() only works if tmp and filename are on same volume
    // and must have an actual filename to call this, which tmpfile() doesn't supply
    // this removes any old file present
    bool success = (rename(_filename.c_str(), dstFilename) == 0);
    
    // Some impls of rename don't work with directories, but that's all the docs say.
    // This is to fix rename also failing on mac/linux if going cross volume, win does copy behind the scenes
    // but using USE_TMPFILEPLUS code above instead on Win.
    if (!success) {
        
        size_t size_ = size();
        vector<uint8_t> tmpBuf;
        tmpBuf.resize(size_);
        
        rewind(_fp);
        if (!read(tmpBuf.data(), size_)) {
            return false;
        }
        // need to fopen file on other volume, then buffer copy the contents over to the over drive
        FileHelper moveHelper;
        if (!moveHelper.open(dstFilename, "w+b")) {
            return false;
        }
        if (!moveHelper.write(tmpBuf.data(), size_)) {
            return false;
        }
        
        // close() should delete the original file
    }
#endif
    // Note: this doesn't change _filename to dstFilename

    return success;
}
*/

bool FileHelper::open(const char* filename, const char* access)
{
    close();

    _fp = fopen(filename, access);
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

int64_t FileHelper::size() const
{
    // returns -1, so can't return size_t
    if (!_fp) {
        return -1;
    }

    // otherwise fstat won't extract the size
    fflush(_fp);

    int fd = fileno(_fp);

    struct stat stats;
    if (fstat(fd, &stats) < 0) {
        return -1;
    }
    return (int64_t)stats.st_size;
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
