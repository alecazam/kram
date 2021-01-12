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
#include "EtcBlock4x4Encoding_R11.h"

namespace Etc
{
	class Block4x4EncodingBits_RG11;

	// ################################################################################
	// Block4x4Encoding_RG11
	// ################################################################################

	class Block4x4Encoding_RG11 : public IBlockEncoding
	{
	public:

		Block4x4Encoding_RG11(void);
		virtual ~Block4x4Encoding_RG11(void);

		virtual void Encode(
			const float *sourcePixels, uint8_t *encodingBits, bool isSnorm) override;

		virtual void Decode(
			uint8_t *encodingBits, const float *sourcePixels, bool isSnorm, uint16_t lastIteration) override;

        virtual void DecodeOnly(
             const uint8_t *encodingBits, float *decodedPixels, bool isSnorm) override;
        
		virtual void PerformIteration(float a_fEffort) override;

		virtual void SetEncodingBits() override;

        virtual bool IsDone() const override { return m_red.IsDone() && m_green.IsDone(); }
        
        // done bit embedded into high bit of each 8-bit count
        // because r and g can be done independently, and with multipass need to skip iteration, though decode/re-encode will occur
        virtual uint16_t GetIterationCount() const override { return m_red.GetIterationCount() + (m_green.GetIterationCount() << 8); }
        
        virtual float GetError() const override { return m_red.GetError() + m_green.GetError(); }
        
    private:
        Block4x4Encoding_R11 m_red;
        Block4x4Encoding_R11 m_green;
	};

	// ----------------------------------------------------------------------------------------------------
	//

} // namespace Etc
