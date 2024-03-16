#pragma once

#import <Foundation/Foundation.h>

typedef struct ZipEntryW {
    const char* _Nonnull filename;  // max 512, aliased
    int32_t fileIndex;

    // attributes
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    int32_t modificationDate;
    uint32_t crc32;
} ZipEntryW;


// Use this to bridge the C++ over to Swift for now
// TODO: form a clang module and reference C++ directly
@interface ZipHelperW : NSObject
    - (nonnull instancetype)initWithData:(nonnull NSData*)data;
    
    // extract the data.  Can alias into the file.
    - (nullable NSData*)extract:(nonnull const char*)filename;
    
    // pass back vector this way for now, should be property
    - (nonnull const ZipEntryW*)zipEntrys;

    - (NSInteger)zipEntrysCount;

    // This isn't the fileIndex, but uses count above to avoid needing to do unsafe
    - (ZipEntryW)zipEntry:(NSInteger)index;

    // retrieve an entry by filename
    - (ZipEntryW)zipEntryByName:(nonnull const char*)name;

@end

// This is only needed for OptFunction and backend names
// Can't prepend extern "C" onto the call.
extern "C" {
const char* _Nonnull demangleSymbolName(const char* _Nonnull symbolName_);
}

