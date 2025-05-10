// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include "KramConfig.h"

namespace kram {

using namespace STL_NAMESPACE;

// Can use to allocate tree nodes where length is unknown
// until the tree is fully parsed.
class BlockedLinearAllocator {
public:
    BlockedLinearAllocator(uint32_t itemsPerBlock, uint32_t itemSize);
    ~BlockedLinearAllocator();

    void* allocate();
    // for POD, caller must zero out?
    template <typename T>
    T* allocateItem()
    {
        return (T*)allocate();
    }

    // placement new/delete could also be done, and variable
    // itemSize, but then have to address alignment

    // Convert to/from an index.  Call before allocate.
    uint32_t nextItemIndex() const { return _counter; }

    // This retrieves data from an index
    void* itemIndexToData(uint32_t itemIndex) const
    {
        uint32_t blockNum = itemIndex / _itemsPerBlock;
        uint32_t blockIndex = itemIndex % _itemsPerBlock;
        return _blocks[blockNum] + blockIndex * _itemSize;
    }

    // can reuse same allocated blocks to avoid fragmentation
    void reset();

    // free the allocated blocks
    void resetAndFree();

    size_t memoryUse() const
    {
        return _blocks.size() * _blockSize;
    }

private:
    bool checkAllocate();

    using Block = uint8_t*;
    vector<Block> _blocks;

    // currently only one item size storeed in Block
    uint32_t _itemSize = 0;
    uint32_t _itemsPerBlock = 0;
    uint32_t _blockSize = 0;

    // where in block, and total item count
    uint32_t _blockCurrent = 0;
    uint32_t _blockCounter = 0; // item index into current block
    uint32_t _counter = 0;
};

} //namespace kram
