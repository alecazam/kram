// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramFileIO.h"

//#include <stdio.h>
//#include "KramFileHelper.h"

//#include <algorithm>

// TODO: move to common header
#define nl "\n"

namespace kram {
using namespace STL_NAMESPACE;
using namespace SIMD_NAMESPACE;

void FileIO::writePad(int paddingSize)
{
    if (paddingSize <= 1) {
        return;
    }

    constexpr int maxPaddingSize = 16;
    constexpr uint8_t padding[maxPaddingSize] = {0};

    paddingSize = std::max(paddingSize, maxPaddingSize);

    size_t dataSize = tell();

    // pad to 16 byte alignment
    size_t valuePadding = (paddingSize - 1) - ((dataSize + (paddingSize - 1)) % paddingSize);

    // write padding out
    writeArray8u(padding, (int)valuePadding);
}

void FileIO::readPad(int paddingSize)
{
    if (paddingSize <= 1) {
        return;
    }
    constexpr int maxPaddingSize = 16;
    uint8_t padding[maxPaddingSize] = {0};

    paddingSize = std::max(paddingSize, maxPaddingSize);

    size_t dataSize = tell();

    // pad to paddingSize
    size_t valuePadding = (paddingSize - 1) - ((dataSize + (paddingSize - 1)) % paddingSize);

    // skip padding
    readArray8u(padding, (int)valuePadding);
}

int FileIO::tell()
{
    if (isFile() && fp) {
        return (int)ftell(fp);
    }
    else if (isData() && _data) {
        return dataLocation;
    }
    else if (isMemory() && mem) {
        return dataLocation;
    }
    else {
        KASSERT(false);
        return 0;
    }
}
void FileIO::seek(int tell_)
{
    if (tell_ < 0) {
        KASSERT(false);
        return;
    }

    if (isFile() && fp) {
        fseek(fp, (size_t)tell_, SEEK_SET);
    }
    else if (isData() && _data) {
        dataLocation = STL_NAMESPACE::clamp(tell_, 0, dataLength);
    }
    else if (isMemory() && mem) {
        dataLocation = STL_NAMESPACE::clamp(tell_, 0, dataLength);
    }
    else {
        KASSERT(false);
    }
}

void FileIO::read(void* data_, int size, int count)
{
    size_t numberOfBytes = size * count;
    if (isFile() && fp) {
        size_t readBytes = fread(data_, 1, numberOfBytes, fp);
        if (readBytes != numberOfBytes) {
            _isFailed = true;
        }
    }
    else if (isData() && _data) {
        if (dataLocation + numberOfBytes <= dataLength) {
            memcpy(data_, _data + dataLocation, numberOfBytes);
            dataLocation += numberOfBytes;
        }
        else {
            _isFailed = true;
        }
    }
    else if (isMemory() && mem) {
        if (dataLocation + numberOfBytes <= dataLength) {
            memcpy(data_, _data + dataLocation, numberOfBytes);
            dataLocation += numberOfBytes;
        }
        else {
            _isFailed = true;
        }
    }
}

void FileIO::write(const void* data_, int size, int count)
{
    if (_isReadOnly) {
        KASSERT(false);
        return;
    }

    int numberOfBytes = size * count;
    if (isFile() && fp) {
        size_t writeBytes = fwrite(data_, 1, numberOfBytes, fp);
        if (writeBytes != numberOfBytes) {
            _isFailed = true;
        }
    }
    else if (isData() && _data) {
        if (dataLocation + numberOfBytes <= dataLength) {
            memcpy(const_cast<uint8_t*>(_data) + dataLocation, data_, numberOfBytes);
            dataLocation += numberOfBytes;
        }
        else {
            _isFailed = true;
        }
    }
    else if (isMemory() && mem) {
        int totalBytes = dataLocation + numberOfBytes;
        if (totalBytes <= dataLength) {
            mem->resize(totalBytes);
            _data = mem->data();
            dataLength = totalBytes;
        }

        // TOOD: handle resize failure?
        
        memcpy(const_cast<uint8_t*>(_data) + dataLocation, data_, numberOfBytes);
        dataLocation += numberOfBytes;
    }
}

} //namespace kram
