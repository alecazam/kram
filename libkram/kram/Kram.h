// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <string>
#include "KramMmapHelper.h"

namespace kram {
using namespace std;

class Image;
class KTXImage;

// This helper needs to stay alive since KTXImage may alias the data.
// KTXImage also has an internal vector already, but fileData may point to the mmap or vector here.
class KTXImageData {
public:
    // class keeps the data alive in mmapHelper or fileData
    bool open(const char* filename, KTXImage& image);
    
    // class aliases data, so caller must keep alive.  Useful with bundle.
    bool open(const uint8_t* data, size_t dataSize, KTXImage& image);
    
    // Open png image into a KTXImage as a single-level mip
    // Only handles 2d case and only srgba/rgba conversion.
    bool openPNG(const char* filename, bool isSrgb, KTXImage& image);

    // This releases all memory associated with this class
    void close();
    
private:
    MmapHelper mmapHelper;
    vector<uint8_t> fileData;
    bool isMmap = false;
    bool isInfoOnly = true;
};

// helpers to source from a png or single level of a ktx
bool LoadKtx(const uint8_t* data, size_t dataSize, Image& sourceImage);
bool LoadPng(const uint8_t* data, size_t dataSize, bool isPremulSrgb, bool isGray, Image& sourceImage);

// can call these with data instead of needing a file
string kramInfoPNGToString(const string& srcFilename, const uint8_t* data, uint64_t dataSize, bool isVerbose);
string kramInfoKTXToString(const string& srcFilename, const KTXImage& srcImage, bool isVerbose);

// return string with data about png/ktx srcFilename
string kramInfoToString(const string& srcFilename, bool isVerbose);

// this is entry point to library for cli app
int32_t kramAppMain(int32_t argc, char* argv[]);
}  // namespace kram
