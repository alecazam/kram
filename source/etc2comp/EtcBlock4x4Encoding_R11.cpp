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
EtcBlock4x4Encoding_R11.cpp

Block4x4Encoding_R11 is the encoder to use when targetting file format R11 and SR11 (signed R11).  

*/

#include "EtcConfig.h"
#include "EtcBlock4x4Encoding_R11.h"

#include "EtcBlock4x4EncodingBits.h"
//#include "EtcBlock4x4.h"

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

    const int MODIFIER_TABLE_ENTRYS = 16;
    const int SELECTOR_BITS = 3;
    const int SELECTORS = 1 << SELECTOR_BITS;

    // modifier values to use for R11, SR11, RG11 and SRG11
    const int8_t s_modifierTable8[MODIFIER_TABLE_ENTRYS][SELECTORS]
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

    // this is simplified for interation
    // stripped down, since it's one of the hotspots of encoding
    inline int DecodePixelRedInt(int baseMul8Plus4, int multiplier, int modifier)
    {
        int pixel = baseMul8Plus4 + modifier * multiplier;
        
        // see here
        // https://www.khronos.org/registry/DataFormat/specs/1.1/dataformat.1.1.html
        
//        if (multiplier > 0)
//        {
//            //fPixel = (a_fBase * 8 + 4) + 8 * fModifier * a_fMultiplier;
//            pixel = baseMul8Plus4 + 8 * modifier * multiplier;
//        }
//        else
//        {
//            //fPixel = (a_fBase * 8 + 4) + fModifier;
//            pixel = baseMul8Plus4 + modifier;
//        }
        
        // just to debug over range pixels
//        if (pixel < 0 || pixel > 2047)
//        {
//            int bp = 0;
//            bp = bp;
//        }
        
        // modifier and multiplier can push base outside valid range, but hw clamps
        pixel = clamp(pixel, 0, 2047);
        return pixel;
    }

	// ----------------------------------------------------------------------------------------------------
	//
	Block4x4Encoding_R11::Block4x4Encoding_R11(void)
	{
		m_pencodingbitsR11 = nullptr;
	}

	Block4x4Encoding_R11::~Block4x4Encoding_R11(void) {}

	// ----------------------------------------------------------------------------------------------------
	void Block4x4Encoding_R11::Encode(
		const float *sourcePixels,
		uint8_t *encodingBits,
        bool isSnorm
    )
	{
        int numSourceChannels = 4; // advance by 4 floats
        
        int fMinRed = 2047;
        int fMaxRed = 0;
                        
        // assumption of unorm float data for sourcePixels here
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            int fRed = clamp((int)roundf(2047.0f * sourcePixels[numSourceChannels * uiPixel]), 0, 2047);
                        
            if (fRed < fMinRed)
            {
                fMinRed = fRed;
            }
            if (fRed > fMaxRed)
            {
                fMaxRed = fRed;
            }
                        
            m_srcPixels[uiPixel] = fRed;
        }
        
        m_redMin = fMinRed;
        m_redMax = fMaxRed;
        
        // now setup for iteration
        m_uiEncodingIterations = 0;
        m_fError = FLT_MAX;
        m_isDone = false;
        m_isSnorm = isSnorm;
        
		m_pencodingbitsR11 = (Block4x4EncodingBits_R11 *)encodingBits;
    }

	// ----------------------------------------------------------------------------------------------------
	void Block4x4Encoding_R11::Decode(
		uint8_t *encodingBits,
		const float *sourcePixels,
        bool isSnorm,
        uint16_t lastIterationCount
    )
	{
        m_isDone = (lastIterationCount & 0x80) != 0; // done high bit
        
        if (m_isDone)
        {
            m_pencodingbitsR11 = nullptr; // skip decode/encode on partially done block
            m_fError = 0.0f;
            return;
        }
    
        m_uiEncodingIterations = lastIterationCount;
        
        // everything is re-established from the encoded block and iteration count
        // since we already have to allocate the block storage, an iteration count per block is only additional
        // also encoders are now across all blocks, so could just allocate one block per thread and iterate until
        // done and skip the priority system.
        //
        // Note: don't call this on done blocks and then iterate, or iteration count will advance
        // m_isDone is set to false in the Encode. Priority queue should ignore done blocks already.
        
		m_pencodingbitsR11 = (Block4x4EncodingBits_R11 *)encodingBits;
        m_isSnorm = isSnorm;
        
        if (m_isSnorm)
        {
            m_redBase = (int16_t)m_pencodingbitsR11->data.base + 128;
        }
        else
        {
            m_redBase = (uint16_t)m_pencodingbitsR11->data.base;
        }
                        
        m_redMultiplier = m_pencodingbitsR11->data.multiplier;
        m_redModifierTableIndex = m_pencodingbitsR11->data.table;

        uint64_t selectorBits = 0;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors0 << (uint64_t)40;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors1 << (uint64_t)32;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors2 << (uint64_t)24;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors3 << (uint64_t)16;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors4 << (uint64_t)8;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors5;
                        
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            uint64_t uiShift = 45 - (3 * uiPixel);
            m_redSelectors[uiPixel] = (selectorBits >> uiShift) & (uint64_t)(SELECTORS - 1);
        }

        // call this to continue encoding later iterations
        Encode(sourcePixels, encodingBits, isSnorm);
    
        // recompute the block error by decoding each pixel
        // could save out error to SortedBlock avoid needing to compute all this
        // but would need to store r and g error separately.
        int blockError = 0;
        
        int baseForDecode = m_redBase * 8 + 4;
        int multiplierForDecode = (m_redMultiplier == 0) ? 1 : (8 * m_redMultiplier);
        
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            int modifier = s_modifierTable8[m_redModifierTableIndex][m_redSelectors[uiPixel]];
           
            int decodedPixelData = DecodePixelRedInt(baseForDecode, multiplierForDecode, modifier);
            
            // add up the error
            int error = decodedPixelData - m_srcPixels[uiPixel];
            blockError += error * error;
        }
    
        m_fError = (float)blockError;
    }
        
    void Block4x4Encoding_R11::DecodeOnly(
        const uint8_t *encodingBits,
        float *decodedPixels,
        bool isSnorm)
    {
        m_pencodingbitsR11 = (Block4x4EncodingBits_R11 *)encodingBits;
        m_isSnorm = isSnorm;
        
        if (m_isSnorm)
        {
            m_redBase = (int16_t)m_pencodingbitsR11->data.base + 128;
        }
        else
        {
            m_redBase = (uint16_t)m_pencodingbitsR11->data.base;
        }
                        
        m_redMultiplier = m_pencodingbitsR11->data.multiplier;
        m_redModifierTableIndex = m_pencodingbitsR11->data.table;

        uint64_t selectorBits = 0;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors0 << (uint64_t)40;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors1 << (uint64_t)32;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors2 << (uint64_t)24;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors3 << (uint64_t)16;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors4 << (uint64_t)8;
        selectorBits |= (uint64_t)m_pencodingbitsR11->data.selectors5;
                        
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            uint64_t uiShift = 45 - (3 * uiPixel);
            m_redSelectors[uiPixel] = (selectorBits >> uiShift) & (uint64_t)(SELECTORS - 1);
        }
        
        // now extract the pixels from the block values above
        int numChannels = 4;
        
        int baseForDecode = m_redBase * 8 + 4;
        int multiplierForDecode = (m_redMultiplier == 0) ? 1 : (8 * m_redMultiplier);
        
        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            int modifier = s_modifierTable8[m_redModifierTableIndex][m_redSelectors[uiPixel]];
            
            int decodedPixelData = DecodePixelRedInt(baseForDecode, multiplierForDecode, modifier);
            
            decodedPixels[uiPixel * numChannels] = decodedPixelData / 2047.0f;
        }
	}

	// ----------------------------------------------------------------------------------------------------
	
    // 16 pixels x 1 unit squared out of 2047
    const float kErrorTolerance = 16 * 1 * 1;

    void Block4x4Encoding_R11::PerformIteration(float a_fEffort)
	{
        if (m_pencodingbitsR11 == nullptr)
        {
            return;
        }
        
        if (m_isDone)
        {
            return;
        }
		
		switch (m_uiEncodingIterations)
		{
		case 0:
			CalculateR11(8, 0, 0);
			break;

		case 1:
			CalculateR11(8, 2, 1);
			if (a_fEffort <= 24.5f) // TODO: decouple effort from this, this is more of an iteration quality
			{
                m_isDone = true;
            }
			break;

		case 2:
			CalculateR11(8, 12, 1);
			if (a_fEffort <= 49.5f)
			{
                m_isDone = true;
            }
			break;

		case 3:
			CalculateR11(7, 6, 1);
			break;

		case 4:
			CalculateR11(6, 3, 1);
			break;

		case 5:
			CalculateR11(5, 1, 0);
			m_isDone = true;
            break;
		}

        // advance to next iteration
        if (!m_isDone)
        {
            if (m_fError < kErrorTolerance)
            {
                m_isDone = true;
            }
            else
            {
                m_uiEncodingIterations++;
            }
        }
	}

	// ----------------------------------------------------------------------------------------------------
	
    // find the best combination of base color, multiplier and selectors
	void Block4x4Encoding_R11::CalculateR11(unsigned int a_uiSelectorsUsed,
												int a_fBaseRadius, int a_fMultiplierRadius)
	{
		// maps from virtual (monotonic) selector to ETC selector
		static const uint8_t auiVirtualSelectorMap[8] = {3, 2, 1, 0, 4, 5, 6, 7};

        // don't search any extra radius if range is 0
        // TODO: there's probably an instant "done" case here without any iteration
		int fRedRange = (m_redMax - m_redMin);
        
        if (fRedRange == 0)
        {
            a_fBaseRadius = 0;
            a_fMultiplierRadius = 0;
        }
        
        // 16 x 8 x 3 x 16 x 16 x 8 iterations = 786K iteraatins / block worst case
        
      	// try each modifier table entry
        // 16 of these
		for (int uiTableEntry = 0; uiTableEntry < MODIFIER_TABLE_ENTRYS; uiTableEntry++)
		{
            // up to 8 of these
			for (int uiMinVirtualSelector = 0;
					uiMinVirtualSelector <= (int)(8 - a_uiSelectorsUsed);
					uiMinVirtualSelector++)
			{
				int uiMaxVirtualSelector = uiMinVirtualSelector + a_uiSelectorsUsed - 1;

				int uiMinSelector = auiVirtualSelectorMap[uiMinVirtualSelector];
				int uiMaxSelector = auiVirtualSelectorMap[uiMaxVirtualSelector];

				int fTableEntryCenter = -s_modifierTable8[uiTableEntry][uiMinSelector];

				int fTableEntryRange = s_modifierTable8[uiTableEntry][uiMaxSelector] -
                                       s_modifierTable8[uiTableEntry][uiMinSelector];

                float fCenterRatio = fTableEntryCenter / (float)fTableEntryRange;
                float fCenter = m_redMin + fCenterRatio * fRedRange;
				int fCenterInt = (int)roundf((255.0f/2047.0f) * fCenter);

                // base of 0 to 255 maps to 0 to 2047
                // search a radius of values off center of range
                int fMinBase = fCenterInt - a_fBaseRadius;
                int fMaxBase = fCenterInt + a_fBaseRadius;
                if (fMinBase < 0)
                {
                    fMinBase = 0;
                }
				if (fMaxBase > 255)
				{
					fMaxBase = 255;
				}

                // 255 / up to 29
                int fRangeMultiplier = (int)roundf((fRedRange * (255.0 / 2047.0f)) / fTableEntryRange);

                int fMinMultiplier = clamp(fRangeMultiplier - a_fMultiplierRadius, 0, 15); // yes, 0
                int fMaxMultiplier = clamp(fRangeMultiplier + a_fMultiplierRadius, 1, 15);
                
                // find best selector for each pixel
                uint8_t bestSelectors[PIXELS];
                int bestRedError[PIXELS];
                
                // only for debug
                //int bestPixelRed[PIXELS];
                
                // up to 3 of these
				for (int fBase = fMinBase; fBase <= fMaxBase; fBase++)
				{
                    int baseForDecode = fBase * 8 + 4;
                    
                    // up to 16 of these
					for (int fMultiplier = fMinMultiplier; fMultiplier <= fMaxMultiplier; fMultiplier++)
					{
                        int multiplierForDecode = (fMultiplier == 0) ? 1 : (8 * fMultiplier);
                        
                        // 16 of these
						for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
						{
                            int bestPixelError = 2047 * 2047;

                            // 8 of these
							for (int uiSelector = 0; uiSelector < SELECTORS; uiSelector++)
							{
                                int modifier = s_modifierTable8[uiTableEntry][uiSelector];
                                
								int fPixelRed = DecodePixelRedInt(baseForDecode, multiplierForDecode, modifier);

                                int error = fPixelRed - (int)m_srcPixels[uiPixel];
                                error *= error;
                                
                                // this is guaranteed to pick one selector for every pixel
                                // the one with the lowest error.
								if (error < bestPixelError)
								{
                                    bestPixelError = error;
                                    bestRedError[uiPixel] = error;
                                    bestSelectors[uiPixel] = uiSelector;
									
									//bestPixelRed[uiPixel] = fPixelRed;
								}
							}
						}
                        
                        // accumulate all best pixel error into block error total
						int blockError = 0;
						for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
						{
                            blockError += bestRedError[uiPixel];
						}
                        
                        // pick that if it's the smallest error
                        if ((float)blockError < m_fError)
						{
							if (m_isSnorm)
							{
								m_redBase = fBase - 128;
							}
							else
							{
                                m_redBase = fBase;
							}
							m_redMultiplier = fMultiplier;
							m_redModifierTableIndex = uiTableEntry;

                            for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
							{
								m_redSelectors[uiPixel] = bestSelectors[uiPixel];
                               
                                // nothing looks at this data, but useful to compare to source
                                //m_decodedPixels[uiPixel] = bestPixelRed[uiPixel]; //  / 2047.0f;
							}
                            
                            // stop if error reached, not going to get any lower
                            m_fError = (float)blockError;
                            
                            // compare to tolerance, since reaching 0 is difficult in float
                            if (m_fError <= kErrorTolerance)
                            {
                                return;
                            }
						}
					}
				}

			}
		}
	}

	// ----------------------------------------------------------------------------------------------------
	// set the encoding bits based on encoding state
	//
	void Block4x4Encoding_R11::SetEncodingBits(void)
	{
        // skip encode if block is already done
        if (m_pencodingbitsR11 == nullptr)
        {
            return;
        }
        
        if (m_isSnorm)
		{
			m_pencodingbitsR11->data.base = (int8_t)m_redBase;
		}
		else
		{
            m_pencodingbitsR11->data.base = (uint8_t)m_redBase;
		}
		m_pencodingbitsR11->data.table = m_redModifierTableIndex;
		m_pencodingbitsR11->data.multiplier = m_redMultiplier;

		uint64_t selectorBits = 0;
		for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
		{
			uint64_t uiShift = 45 - (3 * uiPixel);
			selectorBits |= ((uint64_t)m_redSelectors[uiPixel]) << uiShift;
		}

		m_pencodingbitsR11->data.selectors0 = selectorBits >> (uint64_t)40;
		m_pencodingbitsR11->data.selectors1 = selectorBits >> (uint64_t)32;
		m_pencodingbitsR11->data.selectors2 = selectorBits >> (uint64_t)24;
		m_pencodingbitsR11->data.selectors3 = selectorBits >> (uint64_t)16;
		m_pencodingbitsR11->data.selectors4 = selectorBits >> (uint64_t)8;
		m_pencodingbitsR11->data.selectors5 = selectorBits;        
	}

	// ----------------------------------------------------------------------------------------------------
	//
}
