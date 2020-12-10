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

#include "EtcConfig.h"
//#include "EtcColor.h"

#include <math.h>

namespace Etc
{
    inline float LogToLinear(float a_fLog)
    {
        static const float ALPHA = 0.055f;
        static const float ONE_PLUS_ALPHA = 1.0f + ALPHA;

        if (a_fLog <= 0.04045f)
        {
            return a_fLog / 12.92f;
        }
        else
        {
            return powf((a_fLog + ALPHA) / ONE_PLUS_ALPHA, 2.4f);
        }
    }

    inline float LinearToLog(float a_fLinear)
    {
        static const float ALPHA = 0.055f;
        static const float ONE_PLUS_ALPHA = 1.0f + ALPHA;

        if (a_fLinear <= 0.0031308f)
        {
            return 12.92f * a_fLinear;
        }
        else
        {
            return ONE_PLUS_ALPHA * powf(a_fLinear, (1.0f/2.4f)) - ALPHA;
        }
    }

    class ColorR8G8B8A8
    {
    public:

        uint8_t ucR;
        uint8_t ucG;
        uint8_t ucB;
        uint8_t ucA;

    };

	class ColorFloatRGBA
    {
    public:

		ColorFloatRGBA(void)
        {
            fR = fG = fB = fA = 0.0f;
        }

		ColorFloatRGBA(float a_fR, float a_fG, float a_fB, float a_fA)
        {
            fR = a_fR;
            fG = a_fG;
            fB = a_fB;
            fA = a_fA;
        }

		inline ColorFloatRGBA operator+(const ColorFloatRGBA& a_rfrgba) const
		{
			ColorFloatRGBA frgba;
			frgba.fR = fR + a_rfrgba.fR;
			frgba.fG = fG + a_rfrgba.fG;
			frgba.fB = fB + a_rfrgba.fB;
			frgba.fA = fA + a_rfrgba.fA;
			return frgba;
		}

        inline ColorFloatRGBA operator-(const ColorFloatRGBA& a_rfrgba) const
        {
            ColorFloatRGBA frgba;
            frgba.fR = fR - a_rfrgba.fR;
            frgba.fG = fG - a_rfrgba.fG;
            frgba.fB = fB - a_rfrgba.fB;
            frgba.fA = fA - a_rfrgba.fA;
            return frgba;
        }

        // scalar ops don't apply to alpha
		inline ColorFloatRGBA operator+(float a_f) const
		{
			ColorFloatRGBA frgba;
			frgba.fR = fR + a_f;
			frgba.fG = fG + a_f;
			frgba.fB = fB + a_f;
			frgba.fA = fA;
			return frgba;
		}

        // scalar ops don't apply to alpha
        inline ColorFloatRGBA operator-(float a_f) const
		{
            return *this + (-a_f);
		}

		
        // scalar ops don't apply to alpha
		inline ColorFloatRGBA operator*(float a_f) const
		{
            return ScaleRGB(a_f);
		}

		inline ColorFloatRGBA ScaleRGB(float a_f) const
		{
			ColorFloatRGBA frgba;
			frgba.fR = fR * a_f;
            frgba.fG = fG * a_f;
            frgba.fB = fB * a_f;
			frgba.fA = fA;

			return frgba;
		}

		inline ColorFloatRGBA RoundRGB(void) const
		{
			ColorFloatRGBA frgba;
			frgba.fR = roundf(fR);
			frgba.fG = roundf(fG);
			frgba.fB = roundf(fB);
            frgba.fA = fA; // was missing in original
            
			return frgba;
		}

		inline ColorFloatRGBA ToLinear() const
		{
			ColorFloatRGBA frgbaLinear;
			frgbaLinear.fR = LogToLinear(fR);
			frgbaLinear.fG = LogToLinear(fG);
			frgbaLinear.fB = LogToLinear(fB);
			frgbaLinear.fA = fA;

			return frgbaLinear;
		}

		inline ColorFloatRGBA ToLog(void) const
		{
			ColorFloatRGBA frgbaLog;
			frgbaLog.fR = LinearToLog(fR);
			frgbaLog.fG = LinearToLog(fG);
			frgbaLog.fB = LinearToLog(fB);
			frgbaLog.fA = fA;

			return frgbaLog;
		}

		inline static ColorFloatRGBA ConvertFromRGBA8(uint8_t a_ucR,
			uint8_t a_ucG, uint8_t a_ucB, uint8_t a_ucA)
		{
			ColorFloatRGBA frgba;

			frgba.fR = (float)a_ucR / 255.0f;
			frgba.fG = (float)a_ucG / 255.0f;
			frgba.fB = (float)a_ucB / 255.0f;
			frgba.fA = (float)a_ucA / 255.0f;

			return frgba;
		}

        inline static ColorFloatRGBA ConvertFromRGBA8(const ColorR8G8B8A8& color)
        {
            return ConvertFromRGBA8(color.ucR, color.ucG, color.ucB, color.ucA);
        }
        
		inline static ColorFloatRGBA ConvertFromRGB4(uint8_t a_ucR4,
														uint8_t a_ucG4,
														uint8_t a_ucB4, uint8_t a_ucA = 255)
		{
			uint8_t ucR8 = (uint8_t)((a_ucR4 << 4) + a_ucR4);
			uint8_t ucG8 = (uint8_t)((a_ucG4 << 4) + a_ucG4);
			uint8_t ucB8 = (uint8_t)((a_ucB4 << 4) + a_ucB4);

            return ConvertFromRGBA8(ucR8, ucG8, ucB8, a_ucA);
		}

		inline static ColorFloatRGBA ConvertFromRGB5(uint8_t a_ucR5,
			uint8_t a_ucG5,
			uint8_t a_ucB5, uint8_t a_ucA = 255)
		{
			uint8_t ucR8 = (uint8_t)((a_ucR5 << 3) + (a_ucR5 >> 2));
			uint8_t ucG8 = (uint8_t)((a_ucG5 << 3) + (a_ucG5 >> 2));
			uint8_t ucB8 = (uint8_t)((a_ucB5 << 3) + (a_ucB5 >> 2));

            return ConvertFromRGBA8(ucR8, ucG8, ucB8, a_ucA);
		}

		inline static ColorFloatRGBA ConvertFromR6G7B6(uint8_t a_ucR6,
			uint8_t a_ucG7,
			uint8_t a_ucB6, uint8_t a_ucA = 255)
		{
			uint8_t ucR8 = (uint8_t)((a_ucR6 << 2) + (a_ucR6 >> 4));
			uint8_t ucG8 = (uint8_t)((a_ucG7 << 1) + (a_ucG7 >> 6));
			uint8_t ucB8 = (uint8_t)((a_ucB6 << 2) + (a_ucB6 >> 4));

            return ConvertFromRGBA8(ucR8, ucG8, ucB8, a_ucA);
		}

		// quantize to 4 bits, expand to 8 bits
		inline ColorFloatRGBA QuantizeR4G4B4(void) const
		{
			ColorFloatRGBA frgba = ClampRGB();

			// quantize to 4 bits
			frgba = frgba.ScaleRGB(15.0f).RoundRGB();
			uint32_t uiR4 = (uint32_t)frgba.fR;
            uint32_t uiG4 = (uint32_t)frgba.fG;
            uint32_t uiB4 = (uint32_t)frgba.fB;

            frgba = ConvertFromRGB4(uiR4, uiG4, uiB4);
            frgba.fA = fA;

			return frgba;
		}

		// quantize to 5 bits, expand to 8 bits
		inline ColorFloatRGBA QuantizeR5G5B5(void) const
		{
			ColorFloatRGBA frgba = ClampRGBA();

			// quantize to 5 bits
			frgba = frgba.ScaleRGB(31.0f).RoundRGB();
            uint32_t uiR5 = (uint32_t)frgba.fR;
            uint32_t uiG5 = (uint32_t)frgba.fG;
            uint32_t uiB5 = (uint32_t)frgba.fB;

            frgba = ConvertFromRGB5(uiR5, uiG5, uiB5);
            frgba.fA = fA;
			return frgba;
		}

		// quantize to 6/7/6 bits, expand to 8 bits
		inline ColorFloatRGBA QuantizeR6G7B6(void) const
		{
			ColorFloatRGBA frgba = ClampRGBA();

			// quantize to 6/7/6 bits
			uint32_t uiR6 = (uint32_t)frgba.IntRed(63.0f);
            uint32_t uiG7 = (uint32_t)frgba.IntGreen(127.0f);
            uint32_t uiB6 = (uint32_t)frgba.IntBlue(63.0f);

            frgba = ConvertFromR6G7B6(uiR6, uiG7, uiB6);
            frgba.fA = fA;
            
			return frgba;
		}

		inline ColorFloatRGBA ClampRGB(void) const
		{
            return ClampRGBA();
		}

		inline ColorFloatRGBA ClampRGBA(void) const
		{
			ColorFloatRGBA frgba = *this;
			if (frgba.fR < 0.0f) { frgba.fR = 0.0f; }
			if (frgba.fR > 1.0f) { frgba.fR = 1.0f; }
			if (frgba.fG < 0.0f) { frgba.fG = 0.0f; }
			if (frgba.fG > 1.0f) { frgba.fG = 1.0f; }
			if (frgba.fB < 0.0f) { frgba.fB = 0.0f; }
			if (frgba.fB > 1.0f) { frgba.fB = 1.0f; }
			if (frgba.fA < 0.0f) { frgba.fA = 0.0f; }
			if (frgba.fA > 1.0f) { frgba.fA = 1.0f; }

			return frgba;
		}

		inline int IntRed(float a_fScale) const
		{
			return (int)roundf(fR * a_fScale);
		}

		inline int IntGreen(float a_fScale) const
		{
			return (int)roundf(fG * a_fScale);
		}

		inline int IntBlue(float a_fScale) const
		{
			return (int)roundf(fB * a_fScale);
		}

		inline int IntAlpha(float a_fScale) const
		{
			return (int)roundf(fA * a_fScale);
		}

		float	fR, fG, fB, fA;
    };

}

