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
EtcBlock4x4Encoding.cpp

Block4x4Encoding is the abstract base class for the different encoders.  Each encoder targets a 
particular file format (e.g. ETC1, RGB8, RGBA8, R11)

*/

#include "EtcConfig.h"
#include "EtcBlock4x4Encoding.h"

#include "EtcBlock4x4EncodingBits.h"
#include "EtcBlock4x4.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

namespace Etc
{
	// ----------------------------------------------------------------------------------------------------
	//
	Block4x4Encoding::Block4x4Encoding(void)
	{
        Init();
	}

    void Block4x4Encoding::Init()
    {
        m_pblockParent = nullptr;

        m_pafrgbaSource = nullptr;

        m_fError = 0.0f;

        m_mode = MODE_UNKNOWN;

        m_uiEncodingIterations = 0;
        m_boolDone = false;

        for (int uiPixel = 0; uiPixel < PIXELS; uiPixel++)
        {
            m_afrgbaDecodedColors[uiPixel] = ColorFloatRGBA(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

	// ----------------------------------------------------------------------------------------------------
	// initialize the generic encoding for a 4x4 block
	// a_pblockParent points to the block associated with this encoding
	// a_errormetric is used to choose the best encoding
	// init the decoded pixels to -1 to mark them as undefined
	// init the error to -1 to mark it as undefined
	//
	void Block4x4Encoding::Init(Block4x4 *a_pblockParent,
								const ColorFloatRGBA *a_pafrgbaSource,
								ErrorMetric a_errormetric,
                                uint16_t iterationCount)
	{
        Init();
        
		m_pblockParent = a_pblockParent;
		m_pafrgbaSource = a_pafrgbaSource;
        m_errormetric = a_errormetric;
        
        m_uiEncodingIterations = iterationCount;
	}

    // ----------------------------------------------------------------------------------------------------

    void Block4x4Encoding::SetDoneIfPerfect()
    {
        float kErrorTolerance = 0.0f;

        // instead of comparing to 0 which is almost never achieved in float,
        // use a normalized 8-bit tolerance.  See A8 and R11 code for kErrorTolerance.
#define ADD_TOLERANCE 1
#if ADD_TOLERANCE
        // 16 pixels accumulated, all within 1/255 of final value, and then weights
        static const float kErrorToleranceRec709 = (1.0f / 255.0f) * (1.0f / 255.0f) * 5.0f * 16.0f;
        static const float kErrorToleranceNumeric = (1.0f / 255.0f) * (1.0f / 255.0f) * 3.0f * 16.0f;
        static const float kErrorToleranceGray = (1.0f / 255.0f) * (1.0f / 255.0f) * 1.0f * 16.0f;
        
        switch(m_errormetric)
        {
            case ErrorMetric::GRAY:
                kErrorTolerance = kErrorToleranceGray;
                break;
            case ErrorMetric::NUMERIC:
                kErrorTolerance = kErrorToleranceNumeric;
                break;
            case ErrorMetric::REC709:
                kErrorTolerance = kErrorToleranceRec709;
                break;
        }
#endif
        
        assert(m_fError >= 0.0f);
        if (m_fError <= kErrorTolerance)
        {
            m_boolDone = true;
        }
    }

	// ----------------------------------------------------------------------------------------------------
	//

} // namespace Etc

