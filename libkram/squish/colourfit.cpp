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
   
#include "colourfit.h"
#include "colourset.h"

namespace squish {

ColourFit::ColourFit( ColourSet const* colours, int format, int flags )
  : m_colours( colours ),
    m_format( format ),
	m_flags( flags )
{
}

ColourFit::~ColourFit()
{
}

void ColourFit::Compress( void* block )
{
    // Alec changed to only do 4 color BC1 (not 1a), could add format for BC1a.
    // This is more in line with Crunch which always does 4 color encode.
    // Compress3 is bc1a, but only want to use BC1 for non-alpha (or premul colors)
    // 3 color means only 3 unique colors (and other is transparent).  Only works for premul with alpha = 1
    // 4 color means 2 rgb endpoints without reserved transparent, no 1 bit alpha.
    bool doBC1a = false;
    
	bool isBC1 = (m_format == kBC1);
	if( doBC1a && isBC1 )
	{
        Compress3( block );
		if( !m_colours->IsTransparent() )
			Compress4( block );
	}
	else
		Compress4( block );
}

} // namespace squish
