// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <string>
#include "KramMmapHelper.h"

namespace kram {
using namespace NAMESPACE_STL;

class Image;
class KTXImage;

// This helper needs to stay alive since KTXImage may alias the data.
// KTXImage also has an internal vector already, but fileData may point to the mmap or vector here.
class KTXImageData {
public:
    // class keeps the data alive in mmapHelper or fileData
    bool open(const char* filename, KTXImage& image, bool isInfoOnly_ = false);

    // class aliases data, so caller must keep alive.  Useful with bundle.
    bool open(const uint8_t* data, size_t dataSize, KTXImage& image, bool isInfoOnly_ = false);

    // This releases all memory associated with this class
    void close();

    // Allow caller to set a label that is passed onto created texture.
    // If loaded from filename, then defaults to that.
    void setName(const char* name) { _name = name; }
    const char* name() const { return _name.c_str(); }

private:
    // Open png image into a KTXImage as a single-level mip
    // Only handles 2d case and only srgba/rgba conversion.
    // Only returns non-srgb RGBA8, but format can be changed after for srgb
    bool openPNG(const char* filename, KTXImage& image);

    // The data version
    bool openPNG(const uint8_t* data, size_t dataSize, KTXImage& image);

private:
    string _name;
    MmapHelper mmapHelper;
    vector<uint8_t> fileData;
    bool isMmap = false;
    bool isInfoOnly = false;
};

bool isKTXFilename(const char* filename);
bool isKTX2Filename(const char* filename);
bool isDDSFilename(const char* filename);
bool isPNGFilename(const char* filename);
bool isSupportedFilename(const char* filename);

inline bool isKTXFilename(const string& filename) { return isKTXFilename(filename.c_str()); }
inline bool isKTX2Filename(const string& filename) { return isKTX2Filename(filename.c_str()); }
inline bool isDDSFilename(const string& filename) { return isDDSFilename(filename.c_str()); }
inline bool isPNGFilename(const string& filename) { return isPNGFilename(filename.c_str()); }
inline bool isSupportedFilename(const string& filename) { return isSupportedFilename(filename.c_str()); }

// helpers to source from a png or single level of a ktx
bool LoadKtx(const uint8_t* data, size_t dataSize, Image& sourceImage);
bool LoadPng(const uint8_t* data, size_t dataSize, bool isPremulSrgb, bool isGray, bool& isSrgb, Image& sourceImage);

// can call these with data instead of needing a file
string kramInfoPNGToString(const string& srcFilename, const uint8_t* data, uint64_t dataSize, bool isVerbose);
string kramInfoKTXToString(const string& srcFilename, const KTXImage& srcImage, bool isVerbose);

// return string with data about png/ktx srcFilename
string kramInfoToString(const string& srcFilename, bool isVerbose);

// this is entry point to library for cli app
int32_t kramAppMain(int32_t argc, char* argv[]);

enum TexContentType
{
    TexContentTypeUnknown = 0,
    TexContentTypeAlbedo,
    TexContentTypeNormal,
    TexContentTypeHeight,
    TexContentTypeSDF,
    TexContentTypeAO,
    TexContentTypeMetallicRoughness,
};

// this is a helper to override the format, since sRGB blocks and settings
// often don't indicate the actual color type on PNG, or is omitted
void fixPixelFormat(KTXImage& image, const char* filename);

// This is using naming conventions on filenames, but KTX/KTX2 hold channel props
TexContentType findContentTypeFromFilename(const char* filename);


}  // namespace kram
