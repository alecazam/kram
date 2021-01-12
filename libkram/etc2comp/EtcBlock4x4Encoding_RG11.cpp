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
EtcBlock4x4Encoding_RG11.cpp

Block4x4Encoding_RG11 is the encoder to use when targetting file format RG11 and SRG11 (signed RG11).

*/

#include "EtcConfig.h"
#include "EtcBlock4x4Encoding_RG11.h"

namespace Etc
{
	Block4x4Encoding_RG11::Block4x4Encoding_RG11(void)
	{
	}

	Block4x4Encoding_RG11::~Block4x4Encoding_RG11(void) {}

	void Block4x4Encoding_RG11::Encode(
		const float *sourcePixels, uint8_t *encodingBits, bool isSnorm)
	{
        m_red.Encode(sourcePixels + 0, encodingBits, isSnorm);
        m_green.Encode(sourcePixels + 1, encodingBits + 8, isSnorm);
	}

	void Block4x4Encoding_RG11::Decode(
		unsigned char *encodingBits, const float *sourcePixels, bool isSnorm,
        uint16_t lastIteration)
    {
        m_red.Decode(encodingBits, sourcePixels, isSnorm, (lastIteration >> 0) & 0xFF);
        m_green.Decode(encodingBits + 8, sourcePixels + 1, isSnorm, (lastIteration >> 8) & 0xFF);
 	}

    void Block4x4Encoding_RG11::DecodeOnly(
        const uint8_t *encodingBits, float *decodedPixels, bool isSnorm)
    {
        m_red.DecodeOnly(encodingBits, decodedPixels, isSnorm);
        m_green.DecodeOnly(encodingBits + 8, decodedPixels + 1, isSnorm);
    }

	void Block4x4Encoding_RG11::PerformIteration(float a_fEffort)
	{
        m_red.PerformIteration(a_fEffort);
        m_green.PerformIteration(a_fEffort);
	}

	void Block4x4Encoding_RG11::SetEncodingBits(void)
	{
        m_red.SetEncodingBits();
        m_green.SetEncodingBits();
	}
}
