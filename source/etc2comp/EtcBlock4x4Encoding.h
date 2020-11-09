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

#include "EtcColorFloatRGBA.h"

#include "EtcErrorMetric.h"

#include <assert.h>
#include <float.h>

namespace Etc
{
	class Block4x4;

	// abstract base class only for rgb/a encodings
	class Block4x4Encoding
	{
	public:

		static const int ROWS = 4;
		static const int COLUMNS = 4;
		static const int PIXELS = ROWS * COLUMNS;
		
		typedef enum
		{
			MODE_UNKNOWN,
			//
			MODE_ETC1,
			MODE_T,
			MODE_H,
			MODE_PLANAR,
			MODES
		} Mode;

		Block4x4Encoding(void);
		virtual ~Block4x4Encoding(void) {}
        
		virtual void Encode(Block4x4 *a_pblockParent,
                            const ColorFloatRGBA *a_pafrgbaSource,
                            unsigned char *a_paucEncodingBits,
                            ErrorMetric a_errormetric) = 0;

		virtual void Decode(Block4x4 *a_pblockParent,
                            unsigned char *a_paucEncodingBits,
                            const ColorFloatRGBA *a_pafrgbaSource,
                            ErrorMetric a_errormetric,
                            uint16_t iterationCount) = 0;

        // is is only called on S/RGBA format to copy alpha into decoded pixels of encoding
        virtual void DecodeAlpha() { }
        
		// perform an iteration of the encoding
		// the first iteration must generate a complete, valid (if poor) encoding
		virtual void PerformIteration(float a_fEffort) = 0;

        // write output block
        virtual void SetEncodingBits(void) = 0;

        // the count of the last iteration, can be useful in multipass encoding
        inline uint16_t GetIterationCount(void) const
        {
            return m_uiEncodingIterations;
        }
        
        //-------------------
        
		//void CalcBlockError(void);
        //float CalcPixelError(const ColorFloatRGBA& a_frgbaDecodedColor, int uiPixel) const;
        
		inline float GetError(void) const
		{
			return m_fError;
		}

		inline Mode GetMode(void) const
		{
			return m_mode;
		}

		inline bool IsDone(void) const
		{
			return m_boolDone;
		}

        void SetDoneIfPerfect();

		
        inline const ColorFloatRGBA& GetDecodedPixel(int uiPixel) const
        {
            return m_afrgbaDecodedColors[uiPixel];
        }

        // CalcPixelError is a major hotspot. Called in inner loops.
        // calculate the error between the source pixel and the decoded pixel
        // the error amount is base on the error metric
        inline float CalcPixelError(const ColorFloatRGBA& encodedPixel,
                                                int uiPixel) const
        {

            const ColorFloatRGBA& sourcePixel = m_pafrgbaSource[uiPixel];
            float error = 0.0f;
            
            // don't use alpha in any calcs.  This is only RGB error.
            
            switch(m_errormetric)
            {

            case ErrorMetric::GRAY:
            {
                error = encodedPixel.fR - sourcePixel.fR;
                error *= error;
                
                break;
            }
                    
            case ErrorMetric::REC709:
            case ErrorMetric::NUMERIC:
            {
                float fDX = encodedPixel.fR - sourcePixel.fR;
                float fDY = encodedPixel.fG - sourcePixel.fG;
                float fDZ = encodedPixel.fB - sourcePixel.fB;
                            
                error = fDX*fDX + fDY*fDY + fDZ*fDZ;
                break;
            }
                    
            /* This slows down encoding 28s vs. 20s when not inlined, so stop using it
                also the src isn't cached.
             
            case ErrorMetric::REC709:
            {
                //assert(a_fDecodedAlpha >= 0.0f);

                // YCbCr of source and encodedColor
                // TODO: could cache sourcePixel values to move out of loops
                float fLuma1 = sourcePixel.fR*0.2126f + sourcePixel.fG*0.7152f + sourcePixel.fB*0.0722f;
                float fChromaR1 = (sourcePixel.fR - fLuma1) * (0.5f / (1.0f - 0.2126f));
                float fChromaB1 = (sourcePixel.fB - fLuma1) * (0.5f / (1.0f - 0.0722f));

                float fLuma2 = encodedPixel.fR*0.2126f + encodedPixel.fG*0.7152f + encodedPixel.fB*0.0722f;
                float fChromaR2 = (encodedPixel.fR - fLuma2) * (0.5f / (1.0f - 0.2126f));
                float fChromaB2 = (encodedPixel.fB - fLuma2) * (0.5f / (1.0f - 0.0722f));

                float fDeltaL = fLuma1 - fLuma2;
                float fDeltaCr = fChromaR1 - fChromaR2;
                float fDeltaCb = fChromaB1 - fChromaB2;

                const float LUMA_WEIGHT = 3.0f;
                const float CHROMA_RED_WEIGHT = 1.0f;
                const float CHROMA_BLUE_WEIGHT = 1.0f;
               
                // Favor Luma accuracy over Chroma
                error = LUMA_WEIGHT * fDeltaL*fDeltaL +
                        CHROMA_RED_WEIGHT * fDeltaCr*fDeltaCr +
                        CHROMA_BLUE_WEIGHT * fDeltaCb*fDeltaCb;
        
                break;
            }
            */
            
            }
            
            return error;
        }
        
        // CalcBlockError is a major hotspot. Called in inner loops.
        // calculate the error for the block by summing the pixel errors
        inline void CalcBlockError(void)
        {
            m_fError = 0.0f;

            for (int uiPixel = 0; uiPixel < (int)PIXELS; uiPixel++)
            {
                m_fError += CalcPixelError(m_afrgbaDecodedColors[uiPixel], uiPixel);
            }
        }
        
	protected:
        void Init(Block4x4 *a_pblockParent,
					const ColorFloatRGBA *a_pafrgbaSource,
					ErrorMetric a_errormetric,
                    uint16_t iterationCount);

		Block4x4		*m_pblockParent;
		const ColorFloatRGBA	*m_pafrgbaSource;

		ColorFloatRGBA	m_afrgbaDecodedColors[PIXELS];	// decoded RGB components, ignore Alpha
		float			m_fError;						// error for RGB relative to m_pafrgbaSource.rgb

		// intermediate encoding
		Mode			m_mode;

		unsigned int	m_uiEncodingIterations;
		bool			m_boolDone;						// all iterations have been done
		ErrorMetric		m_errormetric;

	private:
        void Init();
        
	};

} // namespace Etc
