// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "KramConfig.h"

namespace kram {
using namespace NAMESPACE_STL;

class KTXImage;
class FileHelper;

// Help read/write dds files.
// No ASTC or ETC constants, DDS is really only a transport for uncompressed or BC content.
// and isn't as universal as KTX nor can mips be individually supercompressed.
// It also has two variants (DX9 and DX10 era even though Microsoft is on DX12 now).
// Often tools only support the old DX9 era DDS without an explicit DXGI format or array support.
// macOS 11 now supports some DDS formats in Preview.
//
// I'd hoped to avoid this container, but many tools only read/write DDS which stinks.
// I couldn't find any ktx <-> dds converters or they often decompress
// BC-compressed blocks before conversion.
class DDSHelper {
public:
    bool load(const uint8_t* data, size_t dataSize, KTXImage& image, bool isInfoOnly = false);
    bool save(const KTXImage& image, FileHelper& fileHelper);
};

}  // namespace kram
