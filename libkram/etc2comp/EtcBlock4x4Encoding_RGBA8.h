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

#include "EtcBlock4x4Encoding_RGB8.h"

#include "EtcBlock4x4.h" // for SourceAlphaMix

namespace Etc
{
	class Block4x4EncodingBits_A8;

	// ################################################################################
	// Block4x4Encoding_RGBA8
	// RGBA8 if not completely opaque or transparent
	// ################################################################################

    // Encoder for the A8 portion of RGBA.  Minimizes error in a single iteration.
    class Block4x4Encoding_A8
    {
    public:
        Block4x4Encoding_A8(void);
        ~Block4x4Encoding_A8(void);
        
        void Encode(const ColorFloatRGBA *a_pafrgbaSource,
                    unsigned char *a_paucEncodingBits,
                    Block4x4::SourceAlphaMix sourceAlphaMix);
        
        void Decode(unsigned char *a_paucEncodingBits,
                    const ColorFloatRGBA *a_pafrgbaSource);
        
        void DecodeAlpha(float *decodedPixels);
        
        void PerformIteration(float a_fEffort);
        void CalculateA8(int a_fRadius);
        void SetEncodingBits(void);
        
        bool IsDone() const { return m_boolDone; }
        
    private:
        static const int PIXELS = 16;
        
        Block4x4EncodingBits_A8 *m_pencodingbitsA8;
        
        // float* m_afDecodedAlphas; // alias to parent array
        //Block4x4::SourceAlphaMix m_sourceAlphaMix;
        
        const ColorFloatRGBA* m_pafrgbaSource;
        
        uint8_t m_fBase;
        uint8_t m_fMultiplier;
        uint8_t m_uiModifierTableIndex;
        uint8_t m_auiAlphaSelectors[PIXELS];
        
        bool m_boolDone;
    };

    // This basically combines RGBA8 encoder with A8 encoder
	class Block4x4Encoding_RGBA8 : public Block4x4Encoding_RGB8
	{
	public:

		Block4x4Encoding_RGBA8(void);
		virtual ~Block4x4Encoding_RGBA8(void);

		virtual void Encode(Block4x4 *a_pblockParent,
                            const ColorFloatRGBA *a_pafrgbaSource,
                            unsigned char *a_paucEncodingBits, ErrorMetric a_errormetric) override;

		virtual void Decode(Block4x4 *a_pblockParent,
                            unsigned char *a_paucEncodingBits,
                            const ColorFloatRGBA *a_pafrgbaSource,
                            ErrorMetric a_errormetric,
                            uint16_t iterationCount) override;

        virtual void DecodeAlpha() override;
        
		virtual void PerformIteration(float a_fEffort) override;

		virtual void SetEncodingBits(void) override;

	private:
        Block4x4Encoding_A8 m_alpha;
	};

} // namespace Etc
