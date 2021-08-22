// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "KramConfig.h"

// this holds onto the open file and address from mmap operation
class MmapHelper {
public:
    MmapHelper();
    MmapHelper(MmapHelper &&rhs);
    ~MmapHelper();

    bool open(const char *filename);
    void close();

    const uint8_t *data() { return addr; }
    size_t dataLength() { return length; }

private:
    const uint8_t *addr = nullptr;
    size_t length = 0;
};
