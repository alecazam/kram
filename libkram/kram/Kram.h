// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <string>

namespace kram {
using namespace std;

class Image;

// helpers to source from a png or single level of a ktx
bool LoadKtx(const uint8_t* data, size_t dataSize, Image& sourceImage);
bool LoadPng(const uint8_t* data, size_t dataSize, Image& sourceImage);

// return string with data about png/ktx srcFilename
string kramInfoToString(const string& srcFilename, bool isVerbose);

// this is entry point to library for cli app
int32_t kramAppMain(int32_t argc, char* argv[]);
}  // namespace kram
