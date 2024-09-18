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

//#include "EtcColor.h"
#include "EtcColorFloatRGBA.h"
//#include "EtcErrorMetric.h"
#include "EtcImage.h"
#include "EtcBlock4x4Encoding.h"

namespace Etc
{
	//class Block4x4Encoding;
   
    // This base holds a 4x4 block, and is only used for RGB/RGBA encodings
	class Block4x4
	{
	public:

		static const unsigned int ROWS = 4;
		static const unsigned int COLUMNS = 4;
		static const unsigned int PIXELS = ROWS * COLUMNS;

		// the alpha mix for a 4x4 block of pixels
		enum class SourceAlphaMix
		{
			UNKNOWN,
			//
			OPAQUE,			// all 1.0
			TRANSPARENT,	// all channels 0.0
			TRANSLUCENT,	// not all opaque or transparent
            ALL_ZERO_ALPHA  // used for multichannel where all A = 0, but rgb contain data
		};

		typedef void (Block4x4::*EncoderFunctionPtr)(void);

		Block4x4(void);
		~Block4x4();
        
        // called on first init of a block with/without multipass
		void Encode(Image *a_pimageSource,
                    unsigned int a_uiSourceH,
                    unsigned int a_uiSourceV,
                    unsigned char *a_paucEncodingBits
				  );
        
        // used on subsequent passes with multipass to decode from block for subsequent encodes
        void Decode(unsigned int a_uiSourceH,
                    unsigned int a_uiSourceV,
                    unsigned char *a_paucEncodingBits,
                    Image *a_pimageSource,
                    uint16_t iterationCount);

        inline Block4x4Encoding * GetEncoding(void)
        {
            return m_pencoding;
        }
    
        //----------------------
        
        inline unsigned int GetSourceH(void) const
        {
            return m_uiSourceH;
        }

        inline unsigned int GetSourceV(void) const
        {
            return m_uiSourceV;
        }
        
		inline const ColorFloatRGBA * GetSource() const
		{
			return m_afrgbaSource;
        }
                
		inline SourceAlphaMix GetSourceAlphaMix(void) const
		{
			return m_sourcealphamix; // or return from m_pimageSource->GetSourceAlphaMix()
		}

		inline const Image * GetImageSource(void) const
		{
			return m_pimageSource;
		}

		inline bool HasPunchThroughPixels(void) const
		{
			return m_boolPunchThroughPixels;
		}
        
        // gray vs. color
        inline bool HasColorPixels(void) const
        {
            return m_hasColorPixels;
        }

	private:
        Block4x4Encoding* NewEncoderIfNeeded(Image::Format format);
        void Init();
        
		void SetSourcePixels(void);

        static const uint8_t s_auiPixelOrderHScan[PIXELS];

		Image				*m_pimageSource;
		unsigned int		m_uiSourceH;
		unsigned int		m_uiSourceV;
		ColorFloatRGBA		m_afrgbaSource[PIXELS];		// vertical scan (Not std. pixel order, it's stored transposed)

		SourceAlphaMix		m_sourcealphamix;
		bool				m_boolPunchThroughPixels;	// RGB8A1 or SRGB8A1 with any pixels with alpha < 0.5
        bool                m_hasColorPixels;
        
		Block4x4Encoding	*m_pencoding;

	};

} // namespace Etc
