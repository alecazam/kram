// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <string>

namespace kram {
using namespace std;

class Image;
class KTXImage;

// helpers to source from a png or single level of a ktx
bool LoadKtx(const uint8_t* data, size_t dataSize, Image& sourceImage);
bool LoadPng(const uint8_t* data, size_t dataSize, Image& sourceImage);

// can call these with data instead of needing a file
string kramInfoPNGToString(const string& srcFilename, const uint8_t* data, uint64_t dataSize, bool isVerbose);
string kramInfoKTXToString(const string& srcFilename, const KTXImage& srcImage, bool isVerbose);

// return string with data about png/ktx srcFilename
string kramInfoToString(const string& srcFilename, bool isVerbose);

// this is entry point to library for cli app
int32_t kramAppMain(int32_t argc, char* argv[]);
}  // namespace kram
