// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

namespace kram {

class Image;

// helpers to source from a png or single level of a ktx
bool LoadKtx(const uint8_t* data, int dataSize, Image& sourceImage);
bool LoadPng(const uint8_t* data, int dataSize, Image& sourceImage);

// this is entry point to library for cli app
int kramAppMain(int argc, char* argv[]);
}  // namespace kram
