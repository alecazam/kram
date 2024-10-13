#pragma once

#include <span>

#include "KramConfig.h"

struct mz_stream;

namespace kram {
using namespace STL_NAMESPACE;

class FileHelper;

// This can be passed a count
template <typename T>
using Span = span<T, dynamic_extent>;
using Slice = Span<uint8_t>;

// Compressed stream interface.
// Might have gzip, zlib, zip file support
class ICompressedStream {
public:
    virtual ~ICompressedStream() {}

    // compress and store the data
    virtual void compress(const Slice& uncompressedData, bool finish) = 0;

    // when reached then call compress
    virtual uint32_t compressLimit() const = 0;
};

// Compress content into a gzip (.gz) file using deflate.
// The bytes are then written out to a provided FileHelper.
class ZipStream : public ICompressedStream {
public:
    ZipStream();
    virtual ~ZipStream();

    // writes opening header and closing footer
    // Can disable compression for testing the src content.
    bool open(FileHelper* fileHelper, bool isUncompressed = false);
    void close();

    // compress and write data to helper
    virtual void compress(const Slice& uncompressedData, bool finish) override;

    // test this for when to call compress
    virtual uint32_t compressLimit() const override
    {
        return _compressLimit;
    }

private:
    Slice compressSlice(const Slice& in, bool finish);

    vector<uint8_t> _compressed;
    unique_ptr<mz_stream> _stream;
    FileHelper* _fileHelper = nullptr;

    uint32_t _sourceCRC32 = 0;
    size_t _sourceSize = 0;
    uint32_t _compressLimit = 0;
    bool _isUncompressed = false;
};

} // namespace kram
