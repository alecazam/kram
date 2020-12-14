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
EtcBlock4x4.cpp

Implements the state associated with each 4x4 block of pixels in an image

Source images that are not a multiple of 4x4 are extended to fill the Block4x4 using pixels with an 
alpha of NAN

*/

#include "EtcConfig.h"
#include "EtcBlock4x4.h"

//#include "EtcColor.h"
#include "EtcImage.h"
#include "EtcColorFloatRGBA.h"

// only the rgb/a encoders use Block4x4
#include "EtcBlock4x4EncodingBits.h"
#include "EtcBlock4x4Encoding_ETC1.h"
#include "EtcBlock4x4Encoding_RGB8.h"
#include "EtcBlock4x4Encoding_RGBA8.h"
#include "EtcBlock4x4Encoding_RGB8A1.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

namespace Etc
{
	// ETC pixels are scanned vertically.  
	// this mapping is for when someone wants to scan the ETC pixels horizontally
	const uint8_t Block4x4::s_auiPixelOrderHScan[PIXELS] = { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 };

	// ----------------------------------------------------------------------------------------------------
	//
	Block4x4::Block4x4(void)
	{
        Init();
	}
	Block4x4::~Block4x4()
	{
		m_pimageSource = nullptr;
        
		if (m_pencoding)
		{
			delete m_pencoding;
			m_pencoding = nullptr;
		}
	}
    
    void Block4x4::Init() {
        m_pimageSource = nullptr;
        m_pencoding = nullptr;

        m_uiSourceH = 0;
        m_uiSourceV = 0;

        m_sourcealphamix = SourceAlphaMix::UNKNOWN;
        //m_boolBorderPixels = false;
        m_boolPunchThroughPixels = false;
        m_hasColorPixels = false;

        //m_errormetric = ErrorMetric::NUMERIC;
    }

    Block4x4Encoding* Block4x4::NewEncoderIfNeeded(Image::Format format)
    {
        Block4x4Encoding* p_encoding = m_pencoding;
        if (!p_encoding)
        {
            switch(format) {
            case Image::Format::RGB8:
            case Image::Format::SRGB8:
                p_encoding = new Block4x4Encoding_RGB8;
                break;

            case Image::Format::RGBA8:
            case Image::Format::SRGBA8:
                p_encoding = new Block4x4Encoding_RGBA8;
                break;

            // don't really care about using ETC1 or A1
            case Image::Format::ETC1:
                p_encoding = new Block4x4Encoding_ETC1;
                break;
                        
            case Image::Format::RGB8A1:
            case Image::Format::SRGB8A1:
                p_encoding = new Block4x4Encoding_RGB8A1;
                break;
                    
            default:
                assert(false);
                break;
            }
        }
        return p_encoding;
    }

    void Block4x4::Encode(Image *a_pimageSource,
                                    unsigned int a_uiSourceH, unsigned int a_uiSourceV,
                                    unsigned char *a_paucEncodingBits)
    {
        // this is use the same encoding over and over, so don't delete existing
        Etc::Block4x4Encoding* p_encoding = NewEncoderIfNeeded(a_pimageSource->GetFormat());
        ErrorMetric errorMetric = a_pimageSource->GetErrorMetric();
        
        m_pencoding = nullptr;
        Block4x4::Init();
        
        m_pimageSource = a_pimageSource;
        
        m_uiSourceH = a_uiSourceH;
        m_uiSourceV = a_uiSourceV;
        //m_errormetric = errorMetric;
        m_pencoding = p_encoding;
        
        SetSourcePixels();

        m_pencoding->Encode(this, m_afrgbaSource,
                                    a_paucEncodingBits, errorMetric);

    }


	// ----------------------------------------------------------------------------------------------------
	// initialization of encoding state from a prior encoding using encoding bits
	// [a_uiSourceH,a_uiSourceV] is the location of the block in a_pimageSource
	// a_paucEncodingBits is the place to read the prior encoding
	// a_imageformat is used to determine how to interpret a_paucEncodingBits
	// a_errormetric was used for the prior encoding
	//
	void Block4x4::Decode(
											unsigned int a_uiSourceH, unsigned int a_uiSourceV,
											unsigned char *a_paucEncodingBits,
											Image *a_pimageSource,
                                            uint16_t iterationCount)
	{
        // this is use the same encoding over and over, so don't delete existing
        Etc::Block4x4Encoding* p_encoding = NewEncoderIfNeeded(a_pimageSource->GetFormat());
        ErrorMetric errorMetric = a_pimageSource->GetErrorMetric();
        
        //delete m_pencoding;
        m_pencoding = nullptr;
        Block4x4::Init();

		m_pimageSource = a_pimageSource;
        
		m_uiSourceH = a_uiSourceH;
		m_uiSourceV = a_uiSourceV;
		//m_errormetric = errorMetric;
        m_pencoding = p_encoding;

        if (m_pimageSource->HasSourcePixels()) {
            SetSourcePixels();

            m_pencoding->Decode(this, a_paucEncodingBits, m_afrgbaSource, errorMetric, iterationCount);
        }
        else {
            // pure decode
            m_pencoding->Decode(this, a_paucEncodingBits, nullptr, errorMetric, iterationCount);
        }

	}
	
	// ----------------------------------------------------------------------------------------------------
	// set source pixels from m_pimageSource
	// set m_alphamix
	//
	void Block4x4::SetSourcePixels(void)
	{
		// copy source to consecutive memory locations
		// convert from image horizontal scan to block vertical scan
		int uiPixel = 0;
		for (int x = 0; x < 4; x++)
		{
			int uiSourcePixelH = m_uiSourceH + x;

			for (int y = 0; y < 4; y++)
			{
				int uiSourcePixelV = m_uiSourceV + y;

				ColorFloatRGBA pfrgbaSource = m_pimageSource->GetSourcePixel(uiSourcePixelH, uiSourcePixelV);
            
                ColorFloatRGBA& sourcePixel = m_afrgbaSource[uiPixel];
                sourcePixel = pfrgbaSource;
                uiPixel++;
            }
        }
        
        //----------------------------------------
         
        m_hasColorPixels = true;
        for (uiPixel = 0; uiPixel < 16; ++uiPixel)
        {
            ColorFloatRGBA& sourcePixel = m_afrgbaSource[uiPixel];
            
            // this is doing fp equality
            if (sourcePixel.fR != sourcePixel.fG || sourcePixel.fR != sourcePixel.fB)
            {
                m_hasColorPixels = false;
                break;
            }
        }
        
        //----------------------------------------
        
        // alpha census
        int uiTransparentSourcePixels = 0;
        int uiOpaqueSourcePixels = 0;
              
        Image::Format imageformat = m_pimageSource->GetFormat();

        for (uiPixel = 0; uiPixel < 16; ++uiPixel)
        {
            ColorFloatRGBA& sourcePixel = m_afrgbaSource[uiPixel];
            
            // for formats with no alpha, set source alpha to 1
            if (imageformat == Image::Format::ETC1 ||
                imageformat == Image::Format::RGB8 ||
                imageformat == Image::Format::SRGB8)
            {
                sourcePixel.fA = 1.0f;
            }
            
            // for RGB8A1, set source alpha to 0.0 or 1.0
            // set punch through flag
            else if (imageformat == Image::Format::RGB8A1 ||
                     imageformat == Image::Format::SRGB8A1)
            {
                if (sourcePixel.fA >= 0.5f)
                {
                    sourcePixel.fA = 1.0f;
                }
                else
                {
                    sourcePixel.fA = 0.0f;
                    m_boolPunchThroughPixels = true;
                }
            }

            if (sourcePixel.fA == 1.0f)
            {
                uiOpaqueSourcePixels++;
            }
            else if (sourcePixel.fA == 0.0f)
            {
                // TODO: an assumption here that R/G/B are 0, but with multichannel that's not the case
                // A could be all 0, but rgb contain valid channel content
                uiTransparentSourcePixels++;
            }
        }

        // This only applies for RGBA (premul weighted calcs)
		if (uiOpaqueSourcePixels == PIXELS)
		{
			m_sourcealphamix = SourceAlphaMix::OPAQUE;
		}
		else if (uiTransparentSourcePixels == PIXELS)
		{
            // TODO: could check rgb for all 0, and then set TRANSPARENT
            m_sourcealphamix = SourceAlphaMix::TRANSPARENT;
            
            // TODO: nothing setting ALL_ZERO_ALPHA.  Could look at all rgb to identify that.
            
            //(m_pimageSource->GetErrorMetric() == ErrorMetric::NUMERIC || m_pimageSource->GetErrorMetric() == ErrorMetric::RGBX) ? SourceAlphaMix::ALL_ZERO_ALPHA :
            //    SourceAlphaMix::TRANSPARENT;
		}
		else
		{
            m_sourcealphamix = SourceAlphaMix::TRANSLUCENT;
		}

	}

	// ----------------------------------------------------------------------------------------------------
	// return a name for the encoding mode
	//
//	const char * Block4x4::GetEncodingModeName(void)
//	{
//
//		switch (m_pencoding->GetMode())
//		{
//		case Block4x4Encoding::MODE_ETC1:
//			return "ETC1";
//		case Block4x4Encoding::MODE_T:
//			return "T";
//		case Block4x4Encoding::MODE_H:
//			return "H";
//		case Block4x4Encoding::MODE_PLANAR:
//			return "PLANAR";
//		default:
//			return "???";
//		}
//	}

	// ----------------------------------------------------------------------------------------------------
	//

}
