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
    bool isHDRSupportd() const { return _isHDRSupported; }
    
    bool Encode(int metalPixelFormat, int dstDataSize, int blockDimsY,
        bool hasAlpha, bool weightChannels,
        bool isVerbose, int quality,
        int width, int height, const uint8_t* srcData, uint8_t* dstData);
    
    bool Decode(int metalPixelFormat, int dstDataSize, int blockDimsY,
        bool isVerbose,
        int width, int height, const uint8_t* srcData, uint8_t* dstData);

private:
    bool _isBCSupported = false;
    bool _isHDRSupported = false;
};


} // namespace

#endif

