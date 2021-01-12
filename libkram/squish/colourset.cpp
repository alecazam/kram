/* -----------------------------------------------------------------------------

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the 
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to 
	permit persons to whom the Software is furnished to do so, subject to 
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	
   -------------------------------------------------------------------------- */
   
#include "colourset.h"

namespace squish {

ColourSet::ColourSet( u8 const* rgba, int mask, int format, int flags )
  : m_count( 0 ), 
	m_transparent( false )
{
	// check the compression mode for dxt1
	bool isBC1 = format == kBC1;
	bool weightByAlpha = ( ( flags & kWeightColourByAlpha ) != 0 );

	// create the minimal set
	for( int i = 0; i < 16; ++i )
	{
		// check this pixel is enabled
		int bit = 1 << i;
		if( ( mask & bit ) == 0 )
		{
			m_remap[i] = -1;
			continue;
		}
	
        // 128 cutoff for transparency in BC1
        // half alpha above, half below to turn into bitmap
        
        // check for transparent pixels when using dxt1
		if( isBC1 && rgba[4*i + 3] < 128 )
		{
			m_remap[i] = -1;
			m_transparent = true;
			continue;
		}

		// loop over previous points for a match
		for( int j = 0;; ++j )
		{
			// allocate a new point
			if( j == i )
			{
				// normalise coordinates to [0,1]
				float x = ( float )rgba[4*i + 0] / 255.0f;
				float y = ( float )rgba[4*i + 1] / 255.0f;
				float z = ( float )rgba[4*i + 2] / 255.0f;
				
                if (weightByAlpha) {
                    // ensure there is always non-zero weight even for zero alpha
                    float w = ( float )rgba[4*i + 3] / 255.0f;
                    if (rgba[4*i + 3] == 0) {
                        w = 1 / 255.0f;
                    }
                    m_weights[m_count] = w;
                }
                else {
                    m_weights[m_count] = 1.0;
                }
                
				// add the point
				m_points[m_count] = Vec3( x, y, z );
				m_remap[i] = m_count;
				
				// advance
				++m_count;
				break;
			}
		
			// check for a match
			int oldbit = 1 << j;
			bool match = ( ( mask & oldbit ) != 0 )
				&& ( rgba[4*i + 0] == rgba[4*j + 0] )
				&& ( rgba[4*i + 1] == rgba[4*j + 1] )
				&& ( rgba[4*i + 2] == rgba[4*j + 2] )
				&&                  ( rgba[4*j + 3] >= 128 || !isBC1 );
			if( match )
			{
				// get the index of the match
				int index = m_remap[j];
				
                if (weightByAlpha) {
                    float w = ( float )rgba[4*i + 3] / 255.0f;
                    // ensure there is always non-zero weight even for zero alpha
                    if (rgba[4*i + 3] == 0) {
                        w = 1 / 255.0f;
                    }
                    
                    m_weights[index] += w;
                }
                else {
                    m_weights[index] += 1.0;
                }
                
				// map to this point and increase the weight
				m_remap[i] = index;
				break;
			}
		}
	}

	// square root the weights
	for( int i = 0; i < m_count; ++i )
		m_weights[i] = std::sqrt( m_weights[i] );
}

void ColourSet::RemapIndices( u8 const* source, u8* target ) const
{
	for( int i = 0; i < 16; ++i )
	{
		int j = m_remap[i];
		if( j == -1 )
			target[i] = 3;
		else
			target[i] = source[j];
	}
}

} // namespace squish
