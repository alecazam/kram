// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "ImmutableString.h"

namespace kram {

using namespace NAMESPACE_STL;

ImmutableStringPool::ImmutableStringPool(size_t capacity_) {
    capacity = capacity_;
    
    // empty string is always 0.  Only one buffer for now.  Does not grow.
    ImmutableStringInfo info = { 0, (uint16_t)counter++ };
    mem = new char[capacity];
    
    memcpy(mem, &info, sizeof(ImmutableStringInfo));
    mem[sizeof(ImmutableStringInfo)] = 0;
    
    emptyString = mem + sizeof(ImmutableStringInfo);
    
    keyTable.reserve(1024);
    
    // keep aligned to 2B for ImmutableStringInfo
    uint32_t sz = 2;
    size += sz + sizeof(ImmutableStringInfo);
}

ImmutableStringPool::~ImmutableStringPool() {
    delete [] mem;
    mem = nullptr;
}

ImmutableString ImmutableStringPool::getImmutableString(const char* s) {
    if (!s || !*s)
        return emptyString;
    
    // caller passing in an already immutable string in the block
    if (isImmutableString(s))
        return s;
    
    // mutex lock from here on down if hitting from multiple threads
    // this is iterating on map
    mylock lock(mapLock);
    
    // find a block with the string
    auto it = map.find(s);
    if (it != map.end())
        return it->first;
    
    // Keep unique key count under 64K
    const uint32_t kMaxCounter = 64*1024;
    if (counter >= kMaxCounter) {
        KLOGE("ImmutableString", "Pool cannot fit string");
        return emptyString;
    }
    // not found, so need to add to an empty block
    size_t sz = strlen(s) + 1;
    
    
    // see if it will fit the block
    if ((size + sz + sizeof(ImmutableStringInfo))  > capacity) {
        KLOGE("ImmutableString", "Pool cannot fit string length %zu", sz);
        return emptyString;
    }
    
    // uint32_t hash = (uint32_t)map.hash_function()( s ); // or just use fnv1a call ?  unordered_map does cache this?
    ImmutableStringInfo info = { (uint16_t)(counter++), (uint16_t)(sz - 1) }; // hashStr };
    
    // 4B header
    sz += sizeof(ImmutableStringInfo);
    
    // This finds a string from a 2B lookup uisng the info.counter
    keyTable.push_back(size + sizeof(ImmutableStringPool));
    
    // append it
    char* immStr = mem + size;
    
    memcpy(immStr, &info, sizeof(ImmutableStringInfo));
    immStr += sizeof(ImmutableStringInfo);
    memcpy(immStr, s, sz);
    
    // add it into the map
    map[immStr] = size;
    size += sz;
    
    // keep aligned to 2 bytes
    size_t align = alignof(ImmutableStringInfo);
    assert(align == 2);
    if (size & 1)
        ++size;
    
    return immStr;
}

}
