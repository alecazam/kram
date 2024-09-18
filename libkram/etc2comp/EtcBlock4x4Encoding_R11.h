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

#pragma once

//#include "EtcBlock4x4Encoding_RGB8.h"

namespace Etc
{
	class Block4x4EncodingBits_R11;

	// ################################################################################
	// Block4x4Encoding_R11
	// ################################################################################

    // Simpler interface for R11 and RG11 without all the code/data from Block4x4.
    class IBlockEncoding
    {
    public:
        virtual ~IBlockEncoding() {}
        
        // setup block for encoding iteration, isDone() true when finished
        virtual void Encode(
            const float *sourcePixels,uint8_t *encodingBits, bool isSnorm) = 0;
        
        // this is for decoding a block in multipass
        virtual void Decode(
            uint8_t *encodingBits, const float *sourcePixels, bool isSnorm,
            uint16_t lastIterationCount
        ) = 0;
        
        // for decoding a block for display or conversion
        virtual void DecodeOnly(
            const uint8_t *encodingBits, float *decodedPixels, bool isSnorm) = 0;
        
        // iterate to reduce the error
        virtual void PerformIteration(float a_fEffort) = 0;

        // write out block
        virtual void SetEncodingBits(void) = 0;

        // when error is zero, or effort level also limits iteration
        virtual bool IsDone() const = 0;
        
        virtual uint16_t GetIterationCount() const = 0;
        
        virtual float GetError() const = 0;
    };

	class Block4x4Encoding_R11 : public IBlockEncoding
	{
	public:

		Block4x4Encoding_R11(void);
		virtual ~Block4x4Encoding_R11(void);

        // setup block for encoding iteration, isDone() true when finished
		virtual void Encode(
            const float *sourcePixels, uint8_t *encodingBits, bool isSnorm) override;

        // this is for decoding a block in multipass
		virtual void Decode(
			uint8_t *encodingBits, const float *sourcePixels, bool isSnorm,
            uint16_t lastIterationCount) override;

        // for decoding a block for display or conversion
        virtual void DecodeOnly(
            const uint8_t *encodingBits,
            float *decodedPixels,
            bool isSnorm) override;
        
		virtual void PerformIteration(float a_fEffort) override;

		virtual void SetEncodingBits(void) override;

        virtual bool IsDone() const override { return m_isDone; }
        
        // done bit embedded into high bit of each 8-bit count
        virtual uint16_t GetIterationCount() const override
        {
            uint16_t count = m_uiEncodingIterations;
            if (m_isDone)
            {
                count |= 0x80; // done high bit
            }
            return count;
        }
        
        virtual float GetError() const override { return m_fError; }
        
	private:
		void CalculateR11(unsigned int a_uiSelectorsUsed, 
							int a_fBaseRadius, int a_fMultiplierRadius);

        Block4x4EncodingBits_R11 *m_pencodingbitsR11;

        //float m_fRedBlockError;
        
        static const int PIXELS = 16; // 4 * 4
        
        // adding data for block reuse (only set on first iteration)
        int16_t m_srcPixels[PIXELS];
        int16_t m_redMin;
        int16_t m_redMax;
        
        // this can all be encoded/decoded from the EAC block
        int16_t m_redBase;
        int16_t m_redMultiplier;
		uint8_t m_redSelectors[PIXELS];
        uint8_t m_redModifierTableIndex;
        
        bool m_isDone;
        bool m_isSnorm; // shifts fBase by 128
        
        // this is only data needed to reiterate, can decode and build up rest
        uint8_t m_uiEncodingIterations;
        float m_fError; // 22-bits + 4-bits = 26 bits        
	};

	// ----------------------------------------------------------------------------------------------------
	//

} // namespace Etc
