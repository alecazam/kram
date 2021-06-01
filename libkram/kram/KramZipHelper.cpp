#include "KramZipHelper.h"

// these are no longer a dependency
//#include "KramMmapHelper.h"
//#include "KramFileHelper.h"

//#include <sys/mman.h>
//#include <sys/stat.h>
//#include <stdio.h>
//#include <unistd.h>

#include "miniz.h"

namespace kram {

ZipHelper::ZipHelper() {
    
}

ZipHelper::~ZipHelper() {
    close();
}
  
bool ZipHelper::openForRead(const uint8_t* zipData_, uint64_t zipDataSize) { // const char* filename) {
//    mmap = std::make_unique<MmapHelper>();
//    if (!mmap->openForRead(filename)) {
//        close();
//        return false;
//    }
    
    zipData = zipData_;
    
    zip = std::make_unique<mz_zip_archive>();
    mz_zip_zero_struct(zip.get());
    
    mz_uint flags = 0;
    mz_bool success = mz_zip_reader_init_mem(zip.get(), zipData, zipDataSize, flags);
    if (!success) {
        close();
        return false;
    }
    
    initZipEntryTables();
    return true;
}

void ZipHelper::filterExtensions(const vector<string>& extensions) {
    
    vector<ZipEntry> zipEntrysFiltered;
    
    std::copy_if(_zipEntrys.begin(), _zipEntrys.end(), std::back_inserter(zipEntrysFiltered), [&extensions](const auto& zipEntry) {
        for (const auto& ext : extensions) {
            if (endsWithExtension(zipEntry.filename, ext)) {
                return true;
            }
        }
        return false;
    });
    
    _zipEntrys = zipEntrysFiltered;
}

void ZipHelper::close() {
    if (zip != nullptr) {
        mz_zip_end(zip.get());
        zip.reset();
    }
    
//    if (mmap != nullptr) {
//        mmap->close();
//        mmap.reset();
//    }
}


void ZipHelper::initZipEntryTables() {
    int32_t numFiles = mz_zip_reader_get_num_files(zip.get());
    
    // allocate array to hold all filenames in one block of memory
    uint64_t totalFilenameSizes = 0;
    for (int32_t i = 0; i < numFiles; ++i) {
        totalFilenameSizes += mz_zip_reader_get_filename(zip.get(), i, nullptr, 0);
    }
        
    const uint32_t* remappedIndices = mz_zip_reader_sorted_file_indices(zip.get());
    
    allFilenames.resize(totalFilenameSizes);

    // allocate an array with the data from the archive that we care about
    _zipEntrys.resize(numFiles);

    int32_t index = 0;
    uint64_t length = 0;
    
    for (int32_t i = 0; i < numFiles; ++i) {
        uint32_t sortedFileIndex = remappedIndices[i];
        
        // file_stat does quite a bit of work, but only want a few fields out of it
        mz_zip_archive_file_stat stat;
        mz_zip_reader_file_stat(zip.get(), sortedFileIndex, &stat);
        if (stat.m_is_directory || !stat.m_is_supported) {
            continue;
        }
        
        // skipping directories and unsupported items
        // also the ordering here is in filename not fileIndex order
        
        // copy all filenames into fixed storage that's all
        // contguous, so that can alis the strings for lookup
        uint64_t filenameLength = std::min((uint64_t)512, (uint64_t)strlen(stat.m_filename) + 1);
        char *filename = &allFilenames[length];
        strncpy(filename, stat.m_filename, filenameLength);
        length += filenameLength;
                                 
        ZipEntry &zipEntry = _zipEntrys[index];
        zipEntry.fileIndex = stat.m_file_index;
        zipEntry.filename = filename; // can alias
        zipEntry.uncompressedSize = stat.m_uncomp_size;
        zipEntry.compressedSize = stat.m_comp_size;
        zipEntry.modificationDate = (int32_t)stat.m_time; // really a time_t
        
        // TODO: stat.m_time, state.m_crc32
        
        index++;
    }
         
    
    // resize, since entries and filenames were skipped
    // this should change the addresses used above
    allFilenames.resize(length);
    _zipEntrys.resize(index);
}

int32_t ZipHelper::zipEntryIndex(const char* name) const {
    // central directory is sorted, so this does binary search on entries
    return mz_zip_reader_locate_file(zip.get(), name, "", 0);
}

const ZipEntry* ZipHelper::zipEntry(const char* name) const {
    int32_t index = zipEntryIndex(name);
    if (index < 0) {
        return nullptr;
    }
    
    // have to find the zipEntry, have skipped and sorted entries by filename
    // the array build skips directories, so those can throw off the fileIndex
    // TODO: do a binary search here, and don't use mz_zip call?
    
    int32_t numEntries = (int32_t)_zipEntrys.size();
    for (int32_t i = 0; i < numEntries; ++i) {
        if (_zipEntrys[i].fileIndex == index) {
            return &_zipEntrys[i];
        }
    }
    
    return nullptr;
}

bool ZipHelper::extract(const char *filename, std::vector<uint8_t>& buffer) const {
    auto entry = zipEntry(filename);
    if (!entry) {
        return false;
    }
    
    buffer.resize(entry->uncompressedSize);
    if (!extract(entry->fileIndex, buffer.data(), buffer.size())) {
        return false;
    }
    
    return true;
}

bool ZipHelper::extractPartial(const char *filename, std::vector<uint8_t>& buffer) const {
    if (buffer.size() == 0) {
        assert(false);
        return false;
    }

    auto entry = zipEntry(filename);
    if (!entry) {
        return false;
    }
    
    if (buffer.size() > entry->uncompressedSize) {
        return false;
    }
    
    bool success = false;
      
    mz_zip_reader_extract_iter_state* iter = mz_zip_reader_extract_iter_new(zip.get(), entry->fileIndex, 0);
    uint64_t bytesRead = mz_zip_reader_extract_iter_read(iter, buffer.data(), buffer.size());
    if (bytesRead == buffer.size()) {
        success = true;
    }
    mz_zip_reader_extract_iter_free(iter);
    return success;
}


bool ZipHelper::extract(int32_t fileIndex, void* buffer, uint64_t bufferSize) const {
    // TODO: here could use the compression lib with optimized deflate
    
    // this pulls pages from mmap, no allocations
    mz_bool success = mz_zip_reader_extract_to_mem(
        zip.get(), fileIndex, buffer, bufferSize, 0);
    
    /* TODO: alternative using optimized Apple library libCompression
     
       this can do partial compression, so don't check uncompressedSize always
       f.e. can look at first 64-byte header on KTX files which is much faster.
     
     uint64_t a = compression_decode_buffer((uint8_t*)dstBuffer, header.uncompressedSize,
                                         (const uint8_t*)data, header.compressedSize,
                                         NULL, COMPRESSION_ALGORITHM_ZLIB);

    if (a != header.uncompressedSize)
    {
    }
    */
    
    return success;
}

// uncompressed content in the archive can be aliased directly by offset into the archive
bool ZipHelper::extractRaw(const char *filename, const uint8_t** bufferData, uint64_t& bufferDataSize) const
{
    auto entry = zipEntry(filename);
    if (!entry) {
        return false;
    }
    
    mz_zip_archive_file_stat stat;
    mz_zip_reader_file_stat(zip.get(), entry->fileIndex, &stat);
    if (stat.m_is_directory || !stat.m_is_supported) {
        return false;
    }
    
    // this should really be cached with zipEntry data
    const uint8_t* data = mz_zip_reader_get_raw_data(zip.get(), entry->fileIndex);
    if (!data) {
        return false;
    }
    
    *bufferData = data;
    bufferDataSize = stat.m_uncomp_size;
    
    return true;
}



}
