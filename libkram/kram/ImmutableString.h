// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include "KramConfig.h"

#include <mutex>

//-----------------------------
// Add in KramHashHelper.h

namespace kram {

using namespace STL_NAMESPACE;

// case-sensitive fnv1a hash, can pass existing hash to continue a hash
inline uint32_t HashFnv1a(const char* val, uint32_t hash = 0x811c9dc5)
{
    const uint32_t prime = 0x01000193; // 16777619 (32-bit)
    while (*val) {
        hash = (hash * prime) ^ (uint32_t)*val++;
    }
    return hash;
}

// this compares string stored as const char*
struct CompareStrings {
    // Would count and/or hash help?
    // otherwise, this has to walk memory, hash already found bucket
    template <class T>
    bool operator()(const T& x, const T& y) const
    {
        return strcmp(x, y) == 0;
    }

    template <class T>
    size_t operator()(const T& x) const
    {
        // 32-bit hash to uint64 conversion on 64-bit system
        return (size_t)HashFnv1a(x);
    }
};

//--------------

// Pool stores info prior to each key.
struct ImmutableStringInfo {
    uint16_t counter;
    uint16_t length;
    //uint32_t hash;
};
using ImmutableString = const char*;

// Store and retrieve immutable strings.  The address of these never changes.
class ImmutableStringPool {
public:
    ImmutableStringPool(size_t capacity_ = 32 * 1024);
    ~ImmutableStringPool();

    ImmutableString getImmutableString(const char* s);
    string_view getImmutableStringView(const char* s)
    {
        ImmutableString str = getImmutableString(s);
        return string_view(getImmutableString(s), getLength(str));
    }

    // Compress 8B to 2B using counter
    uint16_t getCounter(ImmutableString str) const
    {
        const ImmutableStringInfo* info = ((const ImmutableStringInfo*)(str - sizeof(ImmutableStringInfo)));
        return info->counter;
    }
    // cached strlen of string
    uint16_t getLength(ImmutableString str) const
    {
        const ImmutableStringInfo* info = ((const ImmutableStringInfo*)(str - sizeof(ImmutableStringInfo)));
        return info->length;
    }

    // Can lookup string from counter
    ImmutableString getImmutableString(uint16_t counter_) const
    {
        mylock lock(mapLock);
        return mem + keyTable[counter_];
    }
    string_view getImmutableStringView(uint16_t counter_) const
    {
        mylock lock(mapLock);
        ImmutableString str = mem + keyTable[counter_];
        return string_view(str, getLength(str));
    }

    // Can call outside of mutex if mem never grows.
    bool isImmutableString(const char* s) const
    {
        return s >= mem && s < mem + capacity;
    }

private:
    using mymutex = std::mutex;
    using mylock = std::unique_lock<mymutex>; // or lock_guard?

    mutable mymutex mapLock;

    // Remap strings to immutable strings.
    // Could be unordered_set.
    using ImmutableMap = unordered_map<ImmutableString, uint32_t, CompareStrings, CompareStrings>;
    ImmutableMap map;

    // Can convert keys to 2B using lookup table.  Can grow.
    vector<uint32_t> keyTable;

    // Only has one block of memory right now.
    // This block cannot grow or addresses are all invalidated.
    char* mem = nullptr;
    uint32_t size = 0;
    uint32_t capacity = 0;

    // A count of how many strings are stored
    uint32_t counter = 0;

    ImmutableString emptyString = nullptr;
};

} //namespace kram
