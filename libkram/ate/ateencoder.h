#pragma once

#if COMPILE_ATE

#include <stdint.h>

namespace kram {

class KTXHeader;

// ATE = AppleTextureEncoder.
// This was limited to ASTC 4x4 and 8x8 encoding, but has expanded to include BC1/4/5/7 encoders.
// Available on macOS and iOS, but newer OS is needed for newer encoders.
class ATEEncoder {
public:
    ATEEncoder();
    
    bool isBCSupported() const { return _isBCSupported; }
    bool isHDRDecodeSupported() const { return _isHDRDecodeSupported; }
    
    bool Encode(uint32_t metalPixelFormat, size_t dstDataSize, int32_t blockDimsY,
        bool hasAlpha, bool weightChannels,
        bool isVerbose, int32_t quality,
        int32_t width, int32_t height, const uint8_t* srcData, uint8_t* dstData);
    
    bool Decode(uint32_t metalPixelFormat, size_t dstDataSize, int32_t blockDimsY,
        bool isVerbose,
        int32_t width, int32_t height, const uint8_t* srcData, uint8_t* dstData);

private:
    // encode and decode
    bool _isBCSupported = false;
    
    // astc hdr is only decode currently, also bc hdr formats didn't decode
    bool _isHDRDecodeSupported = false;
};


} // namespace

#endif

