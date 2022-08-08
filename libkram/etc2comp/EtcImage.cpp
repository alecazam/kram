/*
 * Copyright 2015 The Etc2Comp Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
EtcImage.cpp

Image is an array of 4x4 blocks that represent the encoding of the source image

*/


#include "EtcConfig.h"

// is this needed?
//#if ETC_WINDOWS
//#include <windows.h>
//#endif


#include "EtcImage.h"

#include "EtcBlock4x4.h"
#include "EtcBlock4x4EncodingBits.h"

#include "EtcBlock4x4Encoding_R11.h"
#include "EtcBlock4x4Encoding_RG11.h"

#include <stdlib.h>
//#include <algorithm>
#include <ctime>
#include <chrono>
//#include <future>
#include <stdio.h>
#include <string.h>
#include <assert.h>
//#include <vector>

#define ETCCOMP_MIN_EFFORT_LEVEL (0.0f)
#define ETCCOMP_DEFAULT_EFFORT_LEVEL (40.0f)
#define ETCCOMP_MAX_EFFORT_LEVEL (100.0f)

// C++14 implement reverse iteration adaptor
// https://stackoverflow.com/questions/8542591/c11-reverse-range-based-for-loop
//template <typename T>
//struct reverseIterator { T& iterable; };
//
//template <typename T>
//auto begin (reverseIterator<T> w) { return std::rbegin(w.iterable); }
//
//template <typename T>
//auto end (reverseIterator<T> w) { return std::rend(w.iterable); }
//
//template <typename T>
//reverseIterator<T> reverse (T&& iterable) { return { iterable }; }

namespace Etc
{
	// ----------------------------------------------------------------------------------------------------
	// constructor using source image
	Image::Image(Format a_format, const ColorR8G8B8A8 *a_pafSourceRGBA, unsigned int a_uiSourceWidth,
					unsigned int a_uiSourceHeight, 
					ErrorMetric a_errormetric)
	{
		m_encodingStatus = EncodingStatus::SUCCESS;
		m_uiSourceWidth = a_uiSourceWidth;
		m_uiSourceHeight = a_uiSourceHeight;

        int uiExtendedWidth = CalcExtendedDimension((unsigned short)m_uiSourceWidth);
        int uiExtendedHeight = CalcExtendedDimension((unsigned short)m_uiSourceHeight);

		m_uiBlockColumns = uiExtendedWidth >> 2;
		m_uiBlockRows = uiExtendedHeight >> 2;

        m_format = a_format;

        m_encodingbitsformat = DetermineEncodingBitsFormat(m_format);
        int blockSize = Block4x4EncodingBits::GetBytesPerBlock(m_encodingbitsformat);
        m_uiEncodingBitsBytes = GetNumberOfBlocks() * blockSize;
        
		m_paucEncodingBits = nullptr;

		m_errormetric = a_errormetric;
		m_fEffort = 0.0f;

		m_iEncodeTime_ms = 0;

		m_bVerboseOutput = false;
        
        // this can be nullptr
        m_pafrgbaSource = a_pafSourceRGBA;
	}

	// ----------------------------------------------------------------------------------------------------
	//
	Image::~Image(void)
	{
	}

    Image::EncodingStatus Image::EncodeSinglepass(float a_fEffort, uint8_t* outputTexture)
    {
        m_encodingStatus = EncodingStatus::SUCCESS;
        m_fEffort = a_fEffort;
        
        // alias the output etxture
        m_paucEncodingBits = outputTexture;
        
        //--------------------------
        // walk the src image as 4x4 blocks, and complete each block and output it
        int blockSize = Block4x4EncodingBits::GetBytesPerBlock(m_encodingbitsformat);
        uint8_t *outputBlock = outputTexture;
        int totalIterations = 0;
        
        switch(m_format) {
            case Image::Format::R11:
            case Image::Format::SIGNED_R11:
            case Image::Format::RG11:
            case Image::Format::SIGNED_RG11:
            {
                bool isSnorm =
                    m_format == Image::Format::SIGNED_R11 ||
                    m_format == Image::Format::SIGNED_RG11;
                bool isR =
                    m_format == Image::Format::R11 ||
                    m_format == Image::Format::SIGNED_R11;
                
                IBlockEncoding* encoder;
                if (isR)
                    encoder = new Block4x4Encoding_R11;
                else
                    encoder = new Block4x4Encoding_RG11;
                
                ColorFloatRGBA sourcePixels[16];
                
                for (int y = 0; y < (int)m_uiBlockRows; y++)
                {
                    int srcY = y * 4;
                    
                    for (int x = 0; x < (int)m_uiBlockColumns; x++)
                    {
                        int srcX = x * 4;

                        // now pull all pixels for the block, this clamps to edge
                        // NOTE: must convert from image horizontal scan to block vertical scan
                        
                        int uiPixel = 0;
                    
                        for (int xx = 0; xx < 4; xx++)
                        {
                            int srcXX = srcX + xx;

                            for (int yy = 0; yy < 4; yy++)
                            {
                                int srcYY = srcY + yy;

                                ColorFloatRGBA sourcePixel = this->GetSourcePixel(srcXX, srcYY);
                                sourcePixels[uiPixel++] = sourcePixel;
                            }
                        }
                        
                        // encode that block in as many iterations as it takes to finish
                        encoder->Encode(&sourcePixels[0].fR, outputBlock, isSnorm);
                        
                        // TODO: consider iterating red until done, then green for cache reasons
                        while (!encoder->IsDone())
                        {
                            // iterate on lowest error until block is done, or quality iterations reached
                            encoder->PerformIteration(m_fEffort);
                            totalIterations++;
                            
                            // only do the first iteration
                            if (m_fEffort == 0.0) {
                                break;
                            }
                        }
                        
                        // store to etc block
                        encoder->SetEncodingBits();
                        
                        outputBlock += blockSize;
                    }
                }
                
                // this encoder isn't created/held by a block, so must be deleted
                delete encoder;
                
                break;
            }
            default:
            {
                // Handle all the rgb/rgba formats which are much more involved
                
                Block4x4 block;
                Block4x4Encoding* encoder = nullptr;
                
                for (int y = 0; y < (int)m_uiBlockRows; y++)
                {
                    int srcY = y * 4;
                    
                    for (int x = 0; x < (int)m_uiBlockColumns; x++)
                    {
                        int srcX = x * 4;

                        // this block copies out a 4x4 tile from the source image
                        block.Encode(this, srcX, srcY, outputBlock);

                        // this encoder is allodated in first encode, then used for all blocks
                        if (!encoder)
                        {
                            encoder = block.GetEncoding();
                        }
                        
                        while (!encoder->IsDone())
                        {
                            // repeat until block is done, then store data
                            encoder->PerformIteration(m_fEffort);
                            totalIterations++;
                            
                            // only do the first iteration
                            if (m_fEffort == 0.0) {
                                break;
                            }
                        }
                        
                        // convert to etc block bits
                        encoder->SetEncodingBits();
                        
                        outputBlock += blockSize;
                    }
                }
                break;
            }
        }
        if (m_bVerboseOutput)
        {
            KLOGI("EtcComp", "Total iterations %d\n", totalIterations);
        }
        
        // block deletes the encoding, so don't delete here
        
        return m_encodingStatus;
    }

	// ----------------------------------------------------------------------------------------------------
	Image::EncodingStatus Image::Encode(float blockPercent,
                                        float a_fEffort,
                                        uint8_t* outputTexture)
	{

		auto start = std::chrono::steady_clock::now();
        int blockSize = Block4x4EncodingBits::GetBytesPerBlock(m_encodingbitsformat);
        
        m_fEffort = a_fEffort;
        
        // alias the output etxture
        m_paucEncodingBits = outputTexture;
        
        using namespace NAMESPACE_STL;
        
        struct SortedBlock
        {
            //uint8_t lastIteration = 0;
            uint16_t srcX = 0, srcY = 0;
            uint16_t iterationData = 0;
            
            float error = FLT_MAX;
            
            // this must match sort operator below
            bool operator>(const SortedBlock& rhs) const
            {
                return error > rhs.error;
            }
        };
        
        int totalIterations = 0;
        int numberOfBlocks = GetNumberOfBlocks();
        
        vector<SortedBlock> sortedBlocks;
        sortedBlocks.resize(numberOfBlocks);
        
        // now fill out the sorted blocks
        for (int y = 0; y < (int)m_uiBlockRows; ++y)
        {
            int yy = y * m_uiBlockColumns;
            
            for (int x = 0; x < (int)m_uiBlockColumns; ++x)
            {
                sortedBlocks[yy + x].srcX = x;
                sortedBlocks[yy + x].srcY = y;
            }
        }
        
        // NOTE: This is the questionable aspect of this encoder.
        // It stops once say 49% of the blocks have finished.  This means the other 51% may have huge errors
        // compared to the source pixels.  colorMap.d finishes in 1 pass since it's mostly gradients, but
        // other textures need many more passes if content varies.
        //
        // One pass is done on all blocks to encode them all, then only
        // the remaining blocks below this count are processed with the top errors in the sorted array.
        // This number is also computed per mip level, but a change was made spend more time in mip blocks
        // and less to large mips.  But effort also affects how many iterations are performed and that affects quality.
    
        int numBlocksToFinish;
        int minBlocks = 0; // 64*64;
        
        if (numberOfBlocks >= minBlocks)
        {
            numBlocksToFinish = static_cast<unsigned int>(roundf(0.01f * blockPercent * numberOfBlocks));
            
            if (m_bVerboseOutput)
            {
                KLOGI("EtcComp", "Will only finish %d/%d blocks", numBlocksToFinish, numberOfBlocks);
            }
        }
        else
        {
            // do all blocks below a certain count, so mips are fully procesed regardless of effor setting
            numBlocksToFinish = numberOfBlocks;
        }
        
        // iterate on all blocks at least once and possible more iterations
        
        // setup for rgb/a
        Block4x4 block;
        Block4x4Encoding* encoder = nullptr;
       
        // setup for r/rg11
        bool isSnorm =
            m_format == Image::Format::SIGNED_R11 ||
            m_format == Image::Format::SIGNED_RG11;
        bool isR =
            m_format == Image::Format::R11 ||
            m_format == Image::Format::SIGNED_R11;
        bool isRG =
            m_format == Image::Format::RG11 ||
            m_format == Image::Format::SIGNED_RG11;
       
        IBlockEncoding* encoderRG = nullptr;
        if (isR)
            encoderRG = new Block4x4Encoding_R11;
        else if (isRG)
            encoderRG = new Block4x4Encoding_RG11;
        
        ColorFloatRGBA sourcePixels[16];
        
        int pass = 0;
        
        while(true)
        {
            // At the end of encode, blocks are encoded back to the outputTexture
            // that way no additional storage is needed, and only one block per thread
            // is required.  This doesn't do threading, since a process works on one texture.
            for (auto& it : sortedBlocks)
            {
                int srcX = it.srcX;
                int srcY = it.srcY;
            
                uint8_t* outputBlock = outputTexture + (srcY * m_uiBlockColumns + srcX) * blockSize;
    
                if (!encoderRG) {
                    // this block copies out a 4x4 tile from the source image
                    if (pass == 0)
                    {
                        block.Encode(this, srcX * 4, srcY * 4, outputBlock);
                        
                        // encoder is allocated on first encode, then reused for the rest
                        // to multithread, would need one block/encoder per therad
                        if (!encoder)
                        {
                            encoder = block.GetEncoding();
                        }
                    }
                    else
                    {
                        block.Decode(srcX * 4, srcY * 4, outputBlock, this, pass);
                    }

                    // this is one pass
                    encoder->PerformIteration(m_fEffort);
                    totalIterations++;
                    
                    // convert to etc block bits
                    encoder->SetEncodingBits();
                    
                    it.iterationData = pass;
                    it.error = encoder->IsDone() ? 0.0f : encoder->GetError();
                }
                else {
                    // different interface for r/rg11, but same logic as above
                    int uiPixel = 0;
                
                    // this copy is a transpose of the block before encoding
                    for (int xx = 0; xx < 4; xx++)
                    {
                        int srcXX = 4 * srcX + xx;

                        for (int yy = 0; yy < 4; yy++)
                        {
                            int srcYY = 4 * srcY + yy;

                            ColorFloatRGBA sourcePixel = this->GetSourcePixel(srcXX, srcYY);
                            sourcePixels[uiPixel++] = sourcePixel;
                        }
                    }
                    
                    // encode that block in as many iterations as it takes to finish
                    if (pass == 0)
                    {
                        encoderRG->Encode(&sourcePixels[0].fR, outputBlock, isSnorm);
                    }
                    else
                    {
                        encoderRG->Decode(outputBlock, &sourcePixels[0].fR, isSnorm, it.iterationData);
                    }
                
                    encoderRG->PerformIteration(m_fEffort);
                    totalIterations++;
                    
                    // store to etc block
                    encoderRG->SetEncodingBits();
                    
                    it.iterationData = encoderRG->GetIterationCount();
                    it.error = encoderRG->IsDone() ? 0.0f : encoderRG->GetError();
                }
            
                if (it.error == 0.0f)
                {
                    numBlocksToFinish--;
                    
                    // stop once block count reached, but can only stop once all blocks encoded at least once
                    if (pass > 0 && numBlocksToFinish <= 0)
                    {
                        break;
                    }
                }
            }
            
            // stop if min effort level, only process blocks once
            if (m_fEffort <= ETCCOMP_MIN_EFFORT_LEVEL)
            {
                break;
            }
            // stop if any pass finished all the blocks
            if (numBlocksToFinish <= 0)
            {
                break;
            }
            
            // sorts largest errors to front
            std::sort(sortedBlocks.begin(), sortedBlocks.end(), std::greater<SortedBlock>());
            
            // lop off the end of the array where blocks are 0 error or don
            int counter = 0;
            for (int i = (int)sortedBlocks.size()-1; i >= 0; --i)
            {
                if (sortedBlocks[i].error == 0.0f)
                {
                    counter++;
                }
                else
                {
                    break;
                }
            }
            
            sortedBlocks.resize(sortedBlocks.size() - counter);
            pass++;
        }
        
        delete encoderRG;
        
        if (m_bVerboseOutput)
        {
            KLOGI("EtcComp", "Total iterations %d in %d passes\n", totalIterations, pass + 1);
        }
        
        auto end = std::chrono::steady_clock::now();
		std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		m_iEncodeTime_ms = (int)elapsed.count();

		return m_encodingStatus;
	}
	
    Image::EncodingStatus Image::Decode(const uint8_t* etcBlocks, uint8_t* outputTexture)
    {
        // setup for rgb/a
        Block4x4 block;
        Block4x4Encoding* encoder = nullptr;
       
        // setup for r/rg11
        bool isSnorm =
            m_format == Image::Format::SIGNED_R11 ||
            m_format == Image::Format::SIGNED_RG11;
        bool isR =
            m_format == Image::Format::R11 ||
            m_format == Image::Format::SIGNED_R11;
        bool isRG =
            m_format == Image::Format::RG11 ||
            m_format == Image::Format::SIGNED_RG11;
       
        IBlockEncoding* encoderRG = nullptr;
        if (isR)
            encoderRG = new Block4x4Encoding_R11;
        else if (isRG)
            encoderRG = new Block4x4Encoding_RG11;
        
        // initialized to 0 by ctor
        ColorFloatRGBA dstPixels[16];
        
        // r and rg wiil return yzw = 001 and zw = 01, rgb will return a = 1
        for (int i = 0; i < 16; ++i)
        {
            dstPixels[i].fA = 1.0f;
        }
        
        int blockSize = Block4x4EncodingBits::GetBytesPerBlock(m_encodingbitsformat);
        
        for (int yy = 0; yy < (int)m_uiBlockRows; ++yy)
        {
            for (int xx = 0; xx < (int)m_uiBlockColumns; ++xx)
            {
                int srcX = xx;
                int srcY = yy;
                
                const uint8_t* srcBlock = etcBlocks + (srcY * m_uiBlockColumns + srcX) * blockSize;

                if (!encoderRG)
                {
                    // this almost works except alpha on RGBA8 isn't set
                    
                    block.Decode(srcX * 4, srcY * 4, (unsigned char*)srcBlock, this, 0);

                    if (!encoder)
                    {
                        encoder = block.GetEncoding();
                    }
                    if (m_format == Image::Format::RGBA8 ||
                        m_format == Image::Format::SRGBA8)
                    {
                        encoder->DecodeAlpha();
                    }
                    
                    // now extract rgb and a from the encoding
                    for (int i = 0; i < 16; ++i)
                    {
                        dstPixels[i] = encoder->GetDecodedPixel(i);
                    }
                }
                else
                {
                    // this fills out r or rg with float values that are unorm 0 to 1 (even for snorm)
                    encoderRG->DecodeOnly(srcBlock, &dstPixels[0].fR, isSnorm);
                }
                
                // now convert float pixels back to unorm8, don't copy pixels in block outside of w/h bound
                // I don't know if dstPixels array is transposed when decoded or not?
                

                ColorR8G8B8A8* dstPixels8 = (ColorR8G8B8A8*)outputTexture;
                for (int y = 0; y < 4; y++)
                {
                    int yd = y + srcY * 4;
                    if (yd >= (int)m_uiSourceHeight)
                    {
                        break;
                    }
                    
                    for (int x = 0; x < 4; x++)
                    {
                        int xd = x + srcX * 4;
                        if (xd >= (int)m_uiSourceWidth)
                        {
                            continue;
                        }
                        
                        const ColorFloatRGBA& color = dstPixels[x * 4 + y]; // Note: pixel lookup transpose here
                
                        ColorR8G8B8A8& dst = dstPixels8[yd * m_uiSourceWidth + xd];
                        dst.ucR = (uint8_t)color.IntRed(255.0f);
                        dst.ucG = (uint8_t)color.IntGreen(255.0f);
                        dst.ucB = (uint8_t)color.IntBlue(255.0f);
                        dst.ucA = (uint8_t)color.IntAlpha(255.0f);
                    }
                }
            }
        }
        
        delete encoderRG;
        
        return m_encodingStatus;
    }

	// ----------------------------------------------------------------------------------------------------
	// return a string name for a given image format
	//
	const char * Image::EncodingFormatToString(Image::Format a_format)
	{
		switch (a_format)
		{
		case Image::Format::ETC1:
			return "ETC1";
		case Image::Format::RGB8:
			return "RGB8";
		case Image::Format::SRGB8:
			return "SRGB8";

		case Image::Format::RGB8A1:
			return "RGB8A1";
		case Image::Format::SRGB8A1:
			return "SRGB8A1";
		case Image::Format::RGBA8:
			return "RGBA8";
		case Image::Format::SRGBA8:
			return "SRGBA8";

		case Image::Format::R11:
			return "R11";
		case Image::Format::SIGNED_R11:
			return "SIGNED_R11";

		case Image::Format::RG11:
			return "RG11";
		case Image::Format::SIGNED_RG11:
			return "SIGNED_RG11";
		case Image::Format::FORMATS:
		case Image::Format::UNKNOWN:
		default:
			return "UNKNOWN";
		}
	}

	// ----------------------------------------------------------------------------------------------------
	// return a string name for the image's format
	//
	const char * Image::EncodingFormatToString(void) const
	{
		return EncodingFormatToString(m_format);
    }

	// ----------------------------------------------------------------------------------------------------
	// determine the encoding bits format based on the encoding format
	// the encoding bits format is a family of bit encodings that are shared across various encoding formats
	//
	Block4x4EncodingBits::Format Image::DetermineEncodingBitsFormat(Format a_format)
	{
		Block4x4EncodingBits::Format encodingbitsformat;

		// determine encoding bits format from image format
		switch (a_format)
		{
		case Format::ETC1:
		case Format::RGB8:
		case Format::SRGB8:
			encodingbitsformat = Block4x4EncodingBits::Format::RGB8;
			break;

		case Format::RGBA8:
		case Format::SRGBA8:
			encodingbitsformat = Block4x4EncodingBits::Format::RGBA8;
			break;

 		case Format::R11:
		case Format::SIGNED_R11:
			encodingbitsformat = Block4x4EncodingBits::Format::R11;
			break;

		case Format::RG11:
		case Format::SIGNED_RG11:
			encodingbitsformat = Block4x4EncodingBits::Format::RG11;
			break;

		case Format::RGB8A1:
		case Format::SRGB8A1:
			encodingbitsformat = Block4x4EncodingBits::Format::RGB8A1;
			break;

		default:
			encodingbitsformat = Block4x4EncodingBits::Format::UNKNOWN;
			break;
		}

		return encodingbitsformat;
	}

	// ----------------------------------------------------------------------------------------------------
	//

}	// namespace Etc
