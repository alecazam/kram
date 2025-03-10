#pragma once

// TODO: move to KramConfig.h
#define KRAM_MAC 1
#define KRAM_IOS 0
#define STL_NAMESPACE std

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

// from miniz
// had to change miniz from anonymous struct typedef, or can't fwd declare
struct mz_zip_archive;

namespace kram {

//struct MmapHelper;
using namespace STL_NAMESPACE;

struct ZipEntry {
    const char* filename; // max 512, aliased
    int32_t fileIndex;

    // attributes
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    int32_t modificationDate;
    uint32_t crc32;
};

// this does very fast zip archive reading via miniz and mmap
// provides data structures to help lookup content
struct ZipHelper {
    ZipHelper();
    ~ZipHelper();

    bool openForRead(const uint8_t* zipData, uint64_t zipDataSize);
    void close();

    // Only keep entries that match the extensions provided
    void filterExtensions(const vector<string>& extensions);

    // buffer is resized if smaller, can use to lookat headers (f.e. ktx or mod)
    // the zip decodes only the length of the buffer passed in, and this should be small
    // since an iterator is called once to extract data
    bool extractPartial(const char* filename, vector<uint8_t>& buffer) const;

    // must read the entire contents (resizes vector)
    bool extract(const char* filename, vector<uint8_t>& buffer) const;

    // must read the entire contents
    bool extract(const char* filename, uint8_t* bufferData, uint64_t bufferDataSize) const;

    // uncompressed content in the archive like ktx2 files can be aliased directly
    // while referencing this data, don't close mmap() since bufferData is offset into that
    bool extractRaw(const char* filename, const uint8_t** bufferData, uint64_t& bufferDataSize) const;

    const vector<ZipEntry>& zipEntrys() const { return _zipEntrys; }

    const ZipEntry* zipEntry(const char* name) const;

private:
    bool extract(const ZipEntry& fileIndex, void* buffer, uint64_t bufferSize) const;

    void initZipEntryTables();

    // returns -1 if file not found, does binary search off sorted names
    // to find fileIndex, then lookups the array index from that
    int32_t zipEntryIndex(const char* name) const;

private:
    std::unique_ptr<mz_zip_archive> zip;
    vector<ZipEntry> _zipEntrys;

    const uint8_t* zipData; // aliased

    vector<char> allFilenames;
};
} // namespace kram
