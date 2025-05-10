// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "BlockedLinearAllocator.h"

namespace kram {

using namespace STL_NAMESPACE;

BlockedLinearAllocator::BlockedLinearAllocator(uint32_t itemsPerBlock, uint32_t itemSize)
    : _itemSize(itemSize),
      _itemsPerBlock(itemsPerBlock),
      _blockSize(itemsPerBlock * itemSize)
{
}

BlockedLinearAllocator::~BlockedLinearAllocator()
{
    resetAndFree();
}

void BlockedLinearAllocator::reset()
{
    // don't free the block memory, reuse for next parse
    _blockCurrent = 0;
    _counter = 0;
    _blockCounter = 0;
}

void BlockedLinearAllocator::resetAndFree()
{
    for (auto& it : _blocks) {
        delete[] it;
    }
    _blocks.clear();
    reset();
}

bool BlockedLinearAllocator::checkAllocate()
{
    // allocate more blocks
    if (_counter >= _blocks.size() * _itemsPerBlock) {
        uint8_t* newBlock = new uint8_t[_blockSize];
        if (!newBlock)
            return false;

        _blocks.push_back(newBlock);
    }

    // advance to next block
    if (_counter && ((_counter % _itemsPerBlock) == 0)) {
        _blockCurrent++;
        _blockCounter = 0;
    }
    return true;
}

void* BlockedLinearAllocator::allocate()
{
    // make sure space exists
    if (!checkAllocate())
        return nullptr;

    // return a new item off the block
    auto& block = _blocks[_blockCurrent];
    uint32_t start = _blockCounter++;
    _counter++;
    return block + start * _itemSize;
}

} //namespace kram
