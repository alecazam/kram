// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <stdint.h>
#include <stdio.h> // for FILE

namespace kram {
using namespace STL_NAMESPACE;
using namespace SIMD_NAMESPACE;

// Unifies binary reads/writes from/to a mmap, buffer, or file pointer.
struct FileIO {
private:
    FILE* fp = nullptr;

    // can point mmap to this
    const uint8_t* _data = nullptr;
    int dataLength = 0;
    int dataLocation = 0;

    bool _isReadOnly = false;
    bool _isResizeable = false;
    bool _isFailed = false;
    
    // dynamic vector
    vector<uint8_t>* mem = nullptr;

public:
    // FileIO doesn't deal with lifetime of the incoming data
    // may eventually have helpers return a FileIO object

    // read/write to file
    FileIO(FILE* fp_, bool isReadOnly = false)
        : fp(fp_), _isReadOnly(isReadOnly), _isResizeable(!isReadOnly)
    {
    }

    // fixed data area for reads/writes, reads are for mmap
    FileIO(const uint8_t* data_, int dataLength_)
        : _data(data_), dataLength(dataLength_), _isReadOnly(true)
    {
    }
    FileIO(uint8_t* data_, int dataLength_)
        : _data(data_), dataLength(dataLength_), _isReadOnly(false)
    {
    }

    // read/write and resizable memory
    FileIO(const vector<uint8_t>* mem_)
        : _data(mem_->data()), dataLength((int)mem_->size()), _isReadOnly(false), _isResizeable(true)
    {
    }
    FileIO(vector<uint8_t>* mem_)
        : mem(mem_), dataLength((int)mem_->size()), _isReadOnly(false), _isResizeable(true)
    {
    }

    bool isFile() const { return fp != nullptr; }
    bool isData() const { return _data != nullptr; }
    bool isMemory() const { return mem != nullptr; }
    bool isFailed() const { return _isFailed; }
    
    void writeArray32u(const uint32_t* data, int count) { write(data, sizeof(uint32_t), count); }
    void writeArray16u(const uint16_t* data, int count) { write(data, sizeof(uint16_t), count); }
    void writeArray8u(const uint8_t* data, int count) { write(data, sizeof(uint8_t), count); }

    void writeArray32i(const int32_t* data, int count) { write(data, sizeof(int32_t), count); }
    void writeArray16i(const int16_t* data, int count) { write(data, sizeof(int16_t), count); }
    void writeArray8i(const int8_t* data, int count) { write(data, sizeof(int8_t), count); }

    // API has to be explicit on writes about type - signed vs. unsigned due to promotion
    // could use const & instead but then can't enforce types written out
    // might switch to writeArray8
    void write32u(uint32_t data) { writeArray32u(&data, 1); }
    void write16u(uint16_t data) { writeArray16u(&data, 1); }
    void write8u(uint8_t data) { writeArray8u(&data, 1); }

    void write32i(int32_t data) { writeArray32i(&data, 1); }
    void write16i(int16_t data) { writeArray16i(&data, 1); }
    void write8i(int8_t data) { writeArray8i(&data, 1); }

    void writeArray32f(const float* data, int count) { write(data, sizeof(float), count); }
    void write32f(float data) { writeArray32f(&data, 1); }

    void writePad(int paddingSize);

    void readPad(int paddingSize);

    // simd too?
#if USE_SIMDLIB
#if SIMD_FLOAT
    void write32fx2(float2 v) { writeArray32fx2(&v, 1); }
    void write32fx3(float3 v) { writeArray32fx3(&v, 1); }
    void write32fx4(float4 v) { writeArray32fx4(&v, 1); }
    
    void writeArray32fx2(const float2* v, int count) { writeArray32f((const float*)v, 2*count); }
    void writeArray32fx3(const float3p* v, int count) { writeArray32f((const float*)v, 3); }
    void writeArray32fx4(const float4* v, int count) { writeArray32f((const float*)v, 4*count); }
    
    // TODO: add read calls
    // TODO: handle float3 to float3p
#endif
    
#if SIMD_INT
    void write32ix2(int2 v) { writeArray32ix2(&v, 1); }
    void write32ix3(int3 v) { writeArray32ix3(&v, 1); }
    void write32ix4(int4 v) { writeArray32ix4(&v, 1); }
    
    void writeArray32ix2(const int2* v, int count) { writeArray32i((const int32_t*)v, 2*count); }
    void writeArray32ix3(const int3p* v, int count) { writeArray32i((const int32_t*)v, 3*count); }
    void writeArray32ix4(const int4* v, int count) { writeArray32i((const int32_t*)v, 4*count); }
    
#endif
 
#if SIMD_SHORT
    void write16ix2(short2 v) { writeArray16ix2(&v, 1); }
    void write16ix3(short3 v) { writeArray16ix3(&v, 1); }
    void write16ix4(short4 v) { writeArray16ix4(&v, 1); }
    
    void writeArray16ix2(const short2* v, int count) { writeArray16i((const short*)v, 2*count); }
    void writeArray16ix3(const short3p* v, int count) { writeArray16i((const short*)v, 3*count); }
    void writeArray16ix4(const short4* v, int count) { writeArray16i((const short*)v, 4*count); }
#endif

#if SIMD_CHAR
    void write8ix2(char2 v) { writeArray8ix2(&v, 1); }
    void write8ix3(char3 v) { writeArray8ix3(&v, 1); }
    void write8ix4(char4 v) { writeArray8ix4(&v, 1); }
    
    void writeArray8ix2(const char2* v, int count) { writeArray8i((const int8_t*)v, 2*count); }
    void writeArray8ix3(const char3p* v, int count) { writeArray8i((const int8_t*)v, 3*count); }
    void writeArray8ix4(const char4* v, int count) { writeArray8i((const int8_t*)v, 4*count); }
#endif
#endif
    
    void readArray32f(float* data, int count) { read(data, sizeof(float), count); }
    void read32f(float& data) { readArray32f(&data, 1); }

    void readArray32u(uint32_t* data, int count) { read(data, sizeof(uint32_t), count); }
    void readArray16u(uint16_t* data, int count) { read(data, sizeof(uint16_t), count); }
    void readArray8u(uint8_t* data, int count) { read(data, sizeof(uint8_t), count); }

    void read32u(uint32_t& data) { readArray32u(&data, 1); }
    void read16u(uint16_t& data) { readArray16u(&data, 1); }
    void read8u(uint8_t& data) { readArray8u(&data, 1); }

    void readArray32i(int32_t* data, int count) { read(data, sizeof(int32_t), count); }
    void readArray16i(int16_t* data, int count) { read(data, sizeof(int16_t), count); }
    void readArray8i(int8_t* data, int count) { read(data, sizeof(int8_t), count); }

    void read32i(int32_t& data) { readArray32i(&data, 1); }
    void read16i(int16_t& data) { readArray16i(&data, 1); }
    void read8i(int8_t& data) { readArray8i(&data, 1); }

    // seek/tell
    int tell();
    void seek(int tell_);

private:
    // binary reads/writes
    void read(void* data_, int size, int count);
    void write(const void* data_, int size, int count);
};

// to better distinguish mmap/buffer io
using DataIO = FileIO;

} //namespace kram
