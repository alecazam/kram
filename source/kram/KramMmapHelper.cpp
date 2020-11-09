// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramMmapHelper.h"

// here's how to mmmap data, but NSData may have another way
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

MmapHelper::MmapHelper() {}

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
}

void MmapHelper::close()
{
    if (addr) {
        munmap((void *)addr, length);
        addr = nullptr;
    }
}
