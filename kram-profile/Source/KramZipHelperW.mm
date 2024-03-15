#include "KramZipHelperW.h"
#include "KramZipHelper.h"

using namespace kram;

@implementation ZipHelperW {
    ZipHelper _helper;
}

- (nonnull instancetype)initWithData:(nonnull NSData*)data {
    _helper.openForRead((const uint8_t*)data.bytes, data.length);
    return self;
}

- (nullable NSData*)extract:(nonnull const char*)filename {
   
    NSData* data = nil;
    
    auto entry = _helper.zipEntry(filename);
    if (!entry) {
        return nil;
    }
    
    bool isCompressed = entry->uncompressedSize != entry->compressedSize;
    if (isCompressed) {
        data = [NSMutableData dataWithLength:entry->uncompressedSize];
        _helper.extract(filename, (uint8_t*)data.bytes, data.length);
    }
    else {
        const uint8_t* bytes = nullptr;
        uint64_t bytesLength = 0;
        
        _helper.extractRaw(filename, &bytes, bytesLength);
        data = [NSData dataWithBytesNoCopy:(void*)bytes length:bytesLength freeWhenDone:NO];
    }
    
    return data;
}

// Need this for the list data
- (nonnull const ZipEntryW*)zipEntrys {
    return (const ZipEntryW*)_helper.zipEntrys().data();
}
- (NSInteger)zipEntrysCount {
    return _helper.zipEntrys().size();
}

- (ZipEntryW)zipEntry:(NSInteger)index {
    return *(const ZipEntryW*)&_helper.zipEntrys()[index];
}

- (ZipEntryW)zipEntryByName:(nonnull const char*)name {
    // TODO: fix to return a dummy type, since zips can be missing files
    // from one iteration to the next.
    return *(const ZipEntryW*)_helper.zipEntry(name);
}


@end

