// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramMmapHelper.h"

// here's how to mmmap data, but NSData has another way
// +dataWithContentsOfURL:options:error: and NSDataReadingMappedIfSafe or NSDataReadingMappedAlways."

#include <stdio.h>
#include <sys/stat.h>

#if KRAM_MAC || KRAM_IOS || KRAM_VISION || KRAM_LINUX
#include <sys/mman.h>
#include <unistd.h>
#elif KRAM_WIN
#include "win_mmap.h"
#endif

MmapHelper::MmapHelper() {}
MmapHelper::MmapHelper(MmapHelper &&rhs)
{
    addr = rhs.addr;
    length = rhs.length;

    // prevent close after move
    rhs.addr = nullptr;
    rhs.length = 0;
}

MmapHelper::~MmapHelper() { close(); }

bool MmapHelper::open(const char *filename)
{
    if (addr) {
        return false;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }
    int32_t fd = fileno(fp);

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fclose(fp);
        return false;
    }
    length = sb.st_size;

    // Only offset needs padded to pagesize, but here offset is always 0

    // Stop padding out to page size, or do but then don't add to length, or will walk too far in memory
    // all remaining page data will be zero, but still want length to reflect actual length of file
    // need Windows equilvent of getpagesize() call before putting this back.  This was to use
    // with MTLBuffer no copy which has a strict page alignment requirement on start and size.
    //
    //#if KRAM_MAC || KRAM_LINUX || KRAM_IOS
    //    // pad it out to the page size (this can be 4k or 16k)
    //    // need this alignment, or it can't be converted to a MTLBuffer
    //    size_t pageSize = FileHelper::pagesize();
    //
    //    size_t padding = (pageSize - 1) - (length + (pageSize - 1)) % pageSize;
    //    if (padding > 0) {
    //        length += padding;
    //    }
    //#endif

    // this needs to be MAP_SHARED or Metal can't reference with NoCopy
    addr =
        (const uint8_t *)mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
    fclose(fp); // mmap keeps pages alive until munmap

    if (addr == MAP_FAILED) {
        return false;
    }
    return true;
}

void MmapHelper::close()
{
    if (addr) {
        munmap((void *)addr, length);
        addr = nullptr;
    }
}
