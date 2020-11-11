// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramMmapHelper.h"

// here's how to mmmap data, but NSData may have another way
#include <stdio.h>
#include <sys/stat.h>

#if KRAM_MAC || KRAM_LINUX
#include <sys/mman.h>
#include <unistd.h>
#endif

MmapHelper::MmapHelper() {}

MmapHelper::~MmapHelper() { close(); }

bool MmapHelper::open(const char *filename)
{
    if (addr) {
        return false;
    }

    // relay on the file api
#if KRAM_WIN
    return false;

    /*
    https://stackoverflow.com/questions/1880714/createfilemapping-mapviewoffile-how-to-avoid-holding-up-the-system-memory
     
    https://www.sublimetext.com/blog/articles/use-mmap-with-care
    https://docs.microsoft.com/en-us/windows/win32/memory/creating-a-view-within-a-file
    
    HANDLE fileHandle = OpenFileMappingA(FILE_MAP_READ, TRUE, filename);
      DWORD  dwDesiredAccess,
      BOOL   bInheritHandle,
      LPCSTR lpName
    );
    
    addr = MapViewOfFile(fileHandle, FILE_MAP_READ, 0, 0, 0);
    if (!addr) {
       return false;
    }
    */

#else

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }
    int fd = fileno(fp);

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fclose(fp);
        return false;
    }
    length = sb.st_size;

    // pad it out to the page size (this can be 4k or 16k)
    // need this alignment, or it can't be converted to a MTLBuffer
    size_t pageSize = getpagesize();

    size_t padding = (pageSize - 1) - (length + (pageSize - 1)) % pageSize;
    if (padding > 0) {
        length += padding;
    }

    addr =
        (const uint8_t *)mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
    fclose(fp);  // mmap keeps pages alive now

    if (addr == MAP_FAILED) {
        return false;
    }
    return true;
#endif
}

void MmapHelper::close()
{
#if KRAM_WIN

#else
    if (addr) {
        munmap((void *)addr, length);
        addr = nullptr;
    }
#endif
}
