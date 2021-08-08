// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <stddef.h>
#include <stdint.h>

//#include <string>

#include "KramConfig.h"

namespace kram {
using namespace NAMESPACE_STL;

// Use this to help open/close files, since dtor is scoped, or caller can close()
// Also allows write to temp file, then rename over the destination file.  This
// avoids leaving unfinished files around when
class FileHelper {
public:
    ~FileHelper();

    bool exists(const char* filenae) const;

    bool open(const char* filename, const char* access);

    // this file is auto-deleted by close(), is that okay with renameFile use?
    bool openTemporaryFile(const char* suffix, const char* access);

    // mainly for tmp files, file can be closed, but this does rename of tmp file.
    // may fail if tmp file and dst are different volumes.
    bool copyTemporaryFileTo(const char* dstFilename);

    void close();

    // returns (size_t)-1 if stat call fails
    size_t size() const;

    FILE* pointer() { return _fp; }

    // safe calls that test bytes read/written
    bool read(uint8_t* data, size_t dataSize);
    bool write(const uint8_t* data, size_t dataSize);

    // if caller only has FILE* then can use these
    static bool readBytes(FILE* fp, uint8_t* data, size_t dataSize);
    static bool writeBytes(FILE* fp, const uint8_t* data, size_t dataSize);

    // return mod stamp on filename
    static uint64_t modificationTimestamp(const char* filename);
        
    static size_t pagesize();

private:
    FILE* _fp = nullptr;
    string _filename;
    bool _isTmpFile = false;
};

}  // namespace kram
