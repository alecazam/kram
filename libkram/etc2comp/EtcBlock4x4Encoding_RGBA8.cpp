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
EtcBlock4x4Encoding_RGBA8.cpp contains:
	Block4x4Encoding_RGBA8
	Block4x4Encoding_RGBA8_Opaque
	Block4x4Encoding_RGBA8_Transparent

These encoders are used when targetting file format RGBA8.

Block4x4Encoding_RGBA8_Opaque is used when all pixels in the 4x4 block are opaque
Block4x4Encoding_RGBA8_Transparent is used when all pixels in the 4x4 block are transparent
Block4x4Encoding_RGBA8 is used when there is a mixture of alphas in the 4x4 block

*/

#include "EtcConfig.h"
#include "EtcBlock4x4Encoding_RGBA8.h"

#include "EtcBlock4x4EncodingBits.h"
#include "EtcBlock4x4.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <limits>
//#include <algorithm>

namespace Etc
{
    template<typename T>
    T clamp(T value, T mn, T mx) {
        return (value <= mn) ? mn : ((value >= mx) ? mx : value);
    }

	// ####################################################################################################
	// Block4x4Encoding_RGBA8
	// ####################################################################################################

    static const unsigned int MODIFIER_TABLE_ENTRYS = 16;
    static const unsigned int ALPHA_SELECTOR_BITS = 3;
    static const unsigned int ALPHA_SELECTORS = 1 << ALPHA_SELECTOR_BITS;

    // same selector table used for R11/G11/A8
    static const int8_t s_aafModifierTable8[MODIFIER_TABLE_ENTRYS][ALPHA_SELECTORS]
    {
        { -3, -6,  -9, -15, 2, 5, 8, 14 },
        { -3, -7, -10, -13, 2, 6, 9, 12 },
        { -2, -5,  -8, -13, 1, 4, 7, 12 },
        { -2, -4,  -6, -13, 1, 3, 5, 12 },

        { -3, -6,  -8, -12, 2, 5, 7, 11 },
        { -3, -7,  -9, -11, 2, 6, 8, 10 },
        { -4, -7,  -8, -11, 3, 6, 7, 10 },
        { -3, -5,  -8, -11, 2, 4, 7, 10 },

        { -2, -6,  -8, -10, 1, 5, 7,  9 },
        { -2, -5,  -8, -10, 1, 4, 7,  9 },
        { -2, -4,  -8, -10, 1, 3, 7,  9 },
        { -2, -5,  -7, -10, 1, 4, 6,  9 },

        { -3, -4,  -7, -10, 2, 3, 6,  9 },
        { -1, -2,  -3, -10, 0, 1, 2,  9 },
        { -4, -6,  -8,  -9, 3, 5, 7,  8 },
        { -3, -5,  -7,  -9, 2, 4, 6,  8 }
    };

    inline float DecodePixelAlpha(float a_fBase, float a_fMultiplier,
                                    unsigned int a_uiTableIndex, unsigned int a_uiSelector)
    {
        float fPixelAlpha = (a_fBase +
                            a_fMultiplier * s_aafModifierTable8[a_uiTableIndex][a_uiSelector]) / 255.0f;
        if (fPixelAlpha < 0.0f)
        {
            fPixelAlpha = 0.0f;
        }
        else if (fPixelAlpha > 1.0f)
        {
            fPixelAlpha = 1.0f;
        }

        return fPixelAlpha;
    }

    inline int DecodePixelAlphaInt(int a_fBase, int a_fMultiplier,
                                    unsigned int a_uiTableIndex, unsigned int a_uiSelector)
    {
        int fPixelAlpha = a_fBase +
                            a_fMultiplier * s_aafModifierTable8[a_uiTableIndex][a_uiSelector];
        
        return clamp(fPixelAlpha, 0, 255);
    }



    Block4x4Encoding_A8::Block4x4Encoding_A8(void)
    {
        m_pencodingbitsA8 = nullptr;
        m_pafrgbaSource = nullptr;
    }

    Block4x4Encoding_A8::~Block4x4Encoding_A8(void) {}

    void Block4x4Encoding_A8::Encode(const ColorFloatRGBA *a_pafrgbaSource,
                                     unsigned char *a_paucEncodingBits,
                                     Block4x4::SourceAlphaMix sourceAlphaMix)
    {
        m_pafrgbaSource = a_pafrgbaSource;
        
        m_boolDone = false;
        
        // really only care about error for one iteration
        //m_fError = FLT_MAX;
        
        m_pencodingbitsA8 = (Block4x4EncodingBits_A8 *)a_paucEncodingBits;
        
        if (sourceAlphaMix == Block4x4::SourceAlphaMix::OPAQUE)
        {
            // set the A8 portion
            m_fBase = 255;
            m_uiModifierTableIndex = 15;
            m_fMultiplier = 15;
            
            // set all selectors to 7 (all bits set)
            for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
            {
                m_auiAlphaSelectors[uiPixel] = 7;
            }

            m_boolDone = true;
        }
        else if ((sourceAlphaMix == Block4x4::SourceAlphaMix::ALL_ZERO_ALPHA) ||
                (sourceAlphaMix == Block4x4::SourceAlphaMix::TRANSPARENT))
        {
            // set the A8 portion
            m_fBase = 0;
            m_uiModifierTableIndex = 0;
            m_fMultiplier = 1;
            
            // set all selectors to 0
            for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
            {
                m_auiAlphaSelectors[uiPixel] = 0;
            }

            m_boolDone = true;
        }
    }

    // A8 always finished in one iterations, but error metrics on rgb iteration may need the alpha values
    // in an error metric. Skip this if alpha not part of the metric.
    void Block4x4Encoding_A8::Decode(unsigned char *a_paucEncodingBits,
                                     const ColorFloatRGBA *a_pafrgbaSource)
    {
        // Note: this is really just decoding to write this exact same data out
        
        m_pafrgbaSource = a_pafrgbaSource; // don't really need to hold this
        m_pencodingbitsA8 = (Block4x4EncodingBits_A8 *)a_paucEncodingBits;
    
        m_fBase = m_pencodingbitsA8->data.base;
        m_fMultiplier = m_pencodingbitsA8->data.multiplier;
        m_uiModifierTableIndex = m_pencodingbitsA8->data.table;
    
        uint64_t ulliSelectorBits = 0;
        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors0 << (uint64_t)40;
        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors1 << (uint64_t)32;
        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors2 << (uint64_t)24;
        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors3 << (uint64_t)16;
        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors4 << (uint64_t)8;
        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors5;
    
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            uint64_t uiShift = 45 - (3 * uiPixel);
            m_auiAlphaSelectors[uiPixel] = (ulliSelectorBits >> uiShift) & (uint64_t)(ALPHA_SELECTORS - 1);
        }
    
        //Encode(a_pafrgbaSource, a_paucEncodingBits, sourceAlphaMix);
        
        // no iteration on A8, it's all done in after first PerformIteration
        m_boolDone = true;
        
        // no error calc since this doesn't iterate, it's already resolved alpha
    }

    void Block4x4Encoding_A8::DecodeAlpha(float* decodedPixels)
    {
//        m_pencodingbitsA8 = (Block4x4EncodingBits_A8 *)a_paucEncodingBits;
//
//        m_fBase = m_pencodingbitsA8->data.base;
//        m_fMultiplier = m_pencodingbitsA8->data.multiplier;
//        m_uiModifierTableIndex = m_pencodingbitsA8->data.table;
//
//        uint64_t ulliSelectorBits = 0;
//        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors0 << (uint64_t)40;
//        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors1 << (uint64_t)32;
//        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors2 << (uint64_t)24;
//        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors3 << (uint64_t)16;
//        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors4 << (uint64_t)8;
//        ulliSelectorBits |= (uint64_t)m_pencodingbitsA8->data.selectors5;
//
//        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
//        {
//            uint64_t uiShift = 45 - (3 * uiPixel);
//            m_auiAlphaSelectors[uiPixel] = (ulliSelectorBits >> uiShift) & (uint64_t)(ALPHA_SELECTORS - 1);
//        }

        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            // this is float version of decode
            float pixel = DecodePixelAlpha(m_fBase, m_fMultiplier,
                m_uiModifierTableIndex,
                m_auiAlphaSelectors[uiPixel]);

            decodedPixels[4 * uiPixel] = pixel;
        }
    }

    void Block4x4Encoding_A8::PerformIteration(float a_fEffort)
    {
        if (m_boolDone)
        {
            return;
        }

        
        // 0, 1, 2 pixel radius all done in iteration 0, only
        // rgb is iterated on over multiple passes.
        if (a_fEffort < 24.9f)
        {
            CalculateA8(0);
        }
        else if (a_fEffort < 49.9f)
        {
            CalculateA8(1);
        }
        else
        {
            CalculateA8(2);
        }
        
        m_boolDone = true;
    }

    void Block4x4Encoding_A8::CalculateA8(int a_fRadius)
    {
        float m_fError = FLT_MAX;
        
        // This code is similiar to CalculateR11.  And it's all very slow doing brute force
        // searches over a large nested for loop space.
        uint8_t srcAlpha[PIXELS];
        
        // find min/max alpha
        int fMinAlpha = 255;
        int fMaxAlpha = 0;
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            int fAlpha = (int)roundf(255.0f * m_pafrgbaSource[uiPixel].fA);
            if (fAlpha < fMinAlpha)
            {
                fMinAlpha = fAlpha;
            }
            if (fAlpha > fMaxAlpha)
            {
                fMaxAlpha = fAlpha;
            }
            
            srcAlpha[uiPixel] = fAlpha;
        }
        
        assert(fMinAlpha >= 0);
        assert(fMaxAlpha <= 255);
        assert(fMinAlpha <= fMaxAlpha);

        int fAlphaRange = fMaxAlpha - fMinAlpha;
        
        // fast path if range 0 (constant alpha), no point in all this iteration
        if (fAlphaRange == 0)
        {
            a_fRadius = 0;
        }
        
        // try each modifier table entry
        //m_fError = FLT_MAX;        // artificially high value
        for (int uiTableEntry = 0; uiTableEntry < (int)MODIFIER_TABLE_ENTRYS; uiTableEntry++)
        {
            static const unsigned int MIN_VALUE_SELECTOR = 3;
            static const unsigned int MAX_VALUE_SELECTOR = 7;

            int fTableEntryCenter = -s_aafModifierTable8[uiTableEntry][MIN_VALUE_SELECTOR];

            int fTableEntryRange = s_aafModifierTable8[uiTableEntry][MAX_VALUE_SELECTOR] -
                s_aafModifierTable8[uiTableEntry][MIN_VALUE_SELECTOR];

            float fCenterRatio = fTableEntryCenter / (float)fTableEntryRange;

            int fCenterInt = (int)roundf(fMinAlpha + fCenterRatio * fAlphaRange);
            //int fCenterInt = roundf(fCenter);

            int fMinBase = fCenterInt - a_fRadius;
            int fMaxBase = fCenterInt + a_fRadius;
            
            if (fMinBase < 0)
            {
                fMinBase = 0;
            }
            if (fMaxBase > 255)
            {
                fMaxBase = 255;
            }

            // 255 range / usp to 29
            int fRangeMultiplier = (int)roundf(fAlphaRange / (float)fTableEntryRange);

            int fMinMultiplier = clamp(fRangeMultiplier - a_fRadius, 1, 15); // no 0 case like on R11
            int fMaxMultiplier = clamp(fRangeMultiplier + a_fRadius, 1, 15);
            
            int auiBestSelectors[PIXELS];
            int afBestAlphaError[PIXELS];
            int afBestDecodedAlphas[PIXELS];
            
            for (int fBase = fMinBase; fBase <= fMaxBase; fBase++)
            {
                for (int fMultiplier = fMinMultiplier; fMultiplier <= fMaxMultiplier; fMultiplier++)
                {
                    // find best selector for each pixel
                    
                    for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
                    {
                        int fBestPixelAlphaError = 255 * 255;
                        for (int uiSelector = 0; uiSelector < (int)ALPHA_SELECTORS; uiSelector++)
                        {
                            int fDecodedAlpha = DecodePixelAlphaInt(fBase, fMultiplier, uiTableEntry, uiSelector);

                            // pixelError = dA ^ 2
                            int fPixelDeltaAlpha = fDecodedAlpha - (int)srcAlpha[uiPixel];
                            int fPixelAlphaError = fPixelDeltaAlpha * fPixelDeltaAlpha;

                            if (fPixelAlphaError < fBestPixelAlphaError)
                            {
                                fBestPixelAlphaError = fPixelAlphaError;
                                auiBestSelectors[uiPixel] = uiSelector;
                                afBestAlphaError[uiPixel] = fBestPixelAlphaError;
                                afBestDecodedAlphas[uiPixel] = fDecodedAlpha;
                            }
                        }
                    }

                    // accumlate pixel error into block error, sum(da^2)
                    int fBlockError = 0;
                    for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
                    {
                        fBlockError += afBestAlphaError[uiPixel];
                    }

                    if (m_fError > (float)fBlockError)
                    {
                        m_fError = (float)fBlockError;

                        m_fBase = fBase;
                        m_fMultiplier = fMultiplier;
                        m_uiModifierTableIndex = uiTableEntry;
                        
                        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
                        {
                            m_auiAlphaSelectors[uiPixel] = auiBestSelectors[uiPixel];
                            
                            //m_afDecodedAlphas[uiPixel] = afBestDecodedAlphas[uiPixel] / 255.0f;
                        }
                        
                        // stop the iteration if tolerance is low enough
                        const int kErrorTolerance = 16 * 1 * 1;
                        if (fBlockError <= kErrorTolerance) {
                            return;
                        }
                    }
                }
            }

        }

    }

    // ----------------------------------------------------------------------------------------------------
    // set the encoding bits based on encoding state
    //
    void Block4x4Encoding_A8::SetEncodingBits(void)
    {
        // set the A8 portion
        m_pencodingbitsA8->data.base = (uint8_t)roundf(/*255.0f * */ m_fBase);
        m_pencodingbitsA8->data.table = m_uiModifierTableIndex;
        m_pencodingbitsA8->data.multiplier = (uint8_t)roundf(m_fMultiplier);

        uint64_t ulliSelectorBits = 0;
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            uint64_t uiShift = 45 - (3 * uiPixel);
            ulliSelectorBits |= ((uint64_t)m_auiAlphaSelectors[uiPixel]) << uiShift;
        }

        m_pencodingbitsA8->data.selectors0 = uint32_t(ulliSelectorBits >> (uint64_t)40);
        m_pencodingbitsA8->data.selectors1 = uint32_t(ulliSelectorBits >> (uint64_t)32);
        m_pencodingbitsA8->data.selectors2 = uint32_t(ulliSelectorBits >> (uint64_t)24);
        m_pencodingbitsA8->data.selectors3 = uint32_t(ulliSelectorBits >> (uint64_t)16);
        m_pencodingbitsA8->data.selectors4 = uint32_t(ulliSelectorBits >> (uint64_t)8);
        m_pencodingbitsA8->data.selectors5 = uint32_t(ulliSelectorBits);
    }


	// ----------------------------------------------------------------------------------------------------
	//
	Block4x4Encoding_RGBA8::Block4x4Encoding_RGBA8(void)
	{
	}
	Block4x4Encoding_RGBA8::~Block4x4Encoding_RGBA8(void) {}

	// ----------------------------------------------------------------------------------------------------
	// initialization prior to encoding
	// a_pblockParent points to the block associated with this encoding
	// a_errormetric is used to choose the best encoding
	// a_pafrgbaSource points to a 4x4 block subset of the source image
	// a_paucEncodingBits points to the final encoding bits
	//
	void Block4x4Encoding_RGBA8::Encode(Block4x4 *a_pblockParent,
												const ColorFloatRGBA *a_pafrgbaSource,
												unsigned char *a_paucEncodingBits, ErrorMetric a_errormetric)
	{
		Block4x4Encoding::Init(a_pblockParent, a_pafrgbaSource, a_errormetric, 0);
        
        // RGB stored after A8 block
        m_pencodingbitsRGB8 = (Block4x4EncodingBits_RGB8 *)(a_paucEncodingBits + 8);

        // Only need alpha channel passed down
        m_alpha.Encode(a_pafrgbaSource, a_paucEncodingBits, a_pblockParent->GetSourceAlphaMix());
	}

	// ----------------------------------------------------------------------------------------------------
	// initialization from the encoding bits of a previous encoding
	// a_pblockParent points to the block associated with this encoding
	// a_errormetric is used to choose the best encoding
	// a_pafrgbaSource points to a 4x4 block subset of the source image
	// a_paucEncodingBits points to the final encoding bits of a previous encoding
	//
	void Block4x4Encoding_RGBA8::Decode(Block4x4 *a_pblockParent,
														unsigned char *a_paucEncodingBits,
														const ColorFloatRGBA *a_pafrgbaSource,
														ErrorMetric a_errormetric,
                                                        uint16_t iterationCount)
	{
        // this won't iterate, but alpha values available for error calc
        // but not using alpha in error calc anymore, so doing after RGB8 decode
        m_alpha.Decode(a_paucEncodingBits, a_pafrgbaSource);
        
		m_pencodingbitsRGB8 = (Block4x4EncodingBits_RGB8 *)(a_paucEncodingBits + 8);

		// init RGB portion
		Block4x4Encoding_RGB8::Decode(a_pblockParent,
													(unsigned char *) m_pencodingbitsRGB8,
													a_pafrgbaSource,
                                                    a_errormetric,
                                                    iterationCount);
	}

    void Block4x4Encoding_RGBA8::DecodeAlpha()
    {
        // API hack toe be able to fill in the decodedPixels from the already Decode called alpha
        // this is so regular Decode path doesn't do this decode and slow down multipass
        m_alpha.DecodeAlpha(&m_afrgbaDecodedColors[0].fA);
    }


	// ----------------------------------------------------------------------------------------------------
	// perform a single encoding iteration
	// replace the encoding if a better encoding was found
	// subsequent iterations generally take longer for each iteration
	// set m_boolDone if encoding is perfect or encoding is finished based on a_fEffort
	//
	// similar to Block4x4Encoding_RGB8_Base::Encode_RGB8(), but with alpha added
	//
	void Block4x4Encoding_RGBA8::PerformIteration(float a_fEffort)
	{
        // return if color and alpha done, note alpha only iterates on 0
        if (m_boolDone && m_alpha.IsDone() )
        {
            return;
        }
        
		if (m_uiEncodingIterations == 0)
		{
            m_alpha.PerformIteration(a_fEffort);
            
            // this skips writing out color too
            if (m_pblockParent->GetSourceAlphaMix() == Block4x4::SourceAlphaMix::TRANSPARENT)
            {
                m_mode = MODE_ETC1;
                m_boolDiff = true;
                m_boolFlip = false;

                // none of these were cleared, like RGBA1 case
                m_uiCW1 = 0;
                m_uiCW2 = 0;

                m_frgbaColor1 = ColorFloatRGBA();
                m_frgbaColor2 = ColorFloatRGBA();
                
                for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
                {
                    m_afrgbaDecodedColors[uiPixel] = ColorFloatRGBA(); // assumes rgb also 0
                    //m_afDecodedAlphas[uiPixel] = 0.0f;
                }

                m_fError = 0.0f;

                // skip processing rgb
                m_boolDone = true;
                //m_uiEncodingIterations++;
            }
		}

        if (!m_boolDone)
        {
            Block4x4Encoding_RGB8::PerformIteration(a_fEffort);
        }

	}

	// ----------------------------------------------------------------------------------------------------
	// set the encoding bits based on encoding state
	//
	void Block4x4Encoding_RGBA8::SetEncodingBits(void)
    {
        // set the RGB8 portion
        Block4x4Encoding_RGB8::SetEncodingBits();
        
        m_alpha.SetEncodingBits();
	}
}
