// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <stddef.h>
#include <stdint.h>

// this holds onto the open file and address from mmap operation
class MmapHelper {
public:
    MmapHelper();
    ~MmapHelper();

    bool open(const char *filename);
    void close();

public:
    const uint8_t *addr = nullptr;
    size_t length = 0;
};
