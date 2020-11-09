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
   
#include "squish.h"
#include "colourset.h"
#include "maths.h"
#include "rangefit.h"
#include "clusterfit.h"
#include "colourblock.h"
#include "alpha.h"
#include "singlecolourfit.h"

namespace squish {

static int FixFlags( int flags )
{
	// grab the flag bits
	int fit = flags & ( kColourIterativeClusterFit | kColourClusterFit | kColourRangeFit );
	int extra = flags & kWeightColourByAlpha;
	
	// set defaults
	if( fit == 0 )
		fit = kColourClusterFit;
		
	// done
	return fit | extra;
}

struct Color {
    uint8_t r, g, b, a;
};

void CompressMasked( u8 * rgba, int mask, void* block, int format, int flags, float const* metric )
{
	// fix any bad flags
	flags = FixFlags( flags );

	// get the block locations
	void* colourBlock = block;
	void* alphaBlock = block;
    
    Color *colors = (Color*)rgba;
    
    if( format == kBC1 | format == kBC2 | format == kBC3 ) {
        // bc2/bc3 write color block after alpha block
        if ( format == kBC2 | format == kBC3 ) {
            colourBlock = reinterpret_cast< u8* >( block ) + 8;
        }
        
        // create the minimal point set
        ColourSet colours( rgba, mask, format, flags );
        int colorCount = colours.GetCount();
        
        // check the compression type and compress colour
        if( colorCount == 1 ) // TODO: could take this for graysccale?
        {
            // always do a single colour fit
            SingleColourFit fit( &colours, format, flags );
            fit.Compress( colourBlock );
        }
        else if( ( flags & kColourRangeFit ) != 0 || colorCount == 0 )
        {
            // do a range fit
            RangeFit fit( &colours, format, flags, metric );
            fit.Compress( colourBlock );
        }
        else
        {
            // default to a cluster fit (could be iterative or not)
            ClusterFit fit( &colours, format, flags, metric );
            fit.Compress( colourBlock );
        }
        
        // compress alpha separately if necessary
        if( format == kBC2 )
            CompressAlphaBC2( rgba, mask, alphaBlock ); // explicit 4-bit
        else if( format == kBC3 )
            CompressAlphaBC3( rgba, mask, alphaBlock ); // 2x8-bit endpoints, 16x3-bit selectors
    }
    else if( format == kBC4 ) {
        // swizzle r to a
        for (int i = 0; i < 16; ++i) {
            colors[i].a = colors[i].r;
        }
        CompressAlphaBC3( rgba, mask, alphaBlock );
    }
    else if( format == kBC5 ) {
        // swizzle r to a
        for (int i = 0; i < 16; ++i) {
            colors[i].a = colors[i].r;
        }
        CompressAlphaBC3( rgba, mask, alphaBlock );
        
        // swizzle g to a
        for (int i = 0; i < 16; ++i) {
            colors[i].a = colors[i].g;
        }
        void* alphaBlockG = reinterpret_cast< u8* >( block ) + 8;
        CompressAlphaBC3( rgba, mask, alphaBlockG );
    }
}


// in bytes
static int GetBlockSize( int format ) {
    if (format == kBC1 | format == kBC4) {
        return 8;
    }
    return 16;
}

void CompressImage( u8 const* rgba, int width, int height, void* blocks, int format, int flags, float const *metric )
{
	// fix any bad flags
	flags = FixFlags( flags );

	// initialise the block output
	u8* targetBlock = reinterpret_cast< u8* >( blocks );
    int bytesPerBlock = GetBlockSize( format );
    
	// loop over blocks
	for( int y = 0; y < height; y += 4 )
	{
		for( int x = 0; x < width; x += 4 )
		{
			// build the 4x4 block of pixels
			u8 sourceRgba[16*4];
			u8* targetPixel = sourceRgba;
			int mask = 0;
			for( int py = 0; py < 4; ++py )
			{
				for( int px = 0; px < 4; ++px )
				{
					// get the source pixel in the image
					int sx = x + px;
					int sy = y + py;
					
					// enable if we're in the image
					if( sx < width && sy < height )
					{
						// copy the rgba value
						u8 const* sourcePixel = rgba + 4*( width*sy + sx );
						for( int i = 0; i < 4; ++i )
							targetPixel[i] = sourcePixel[i];
                        	
						// enable this pixel
						mask |= ( 1 << ( 4*py + px ) );
					}
					//else
					//{
					//	// skip this pixel as its outside the image
                    //}
                    
                    targetPixel += 4;
				}
			}
			
			// compress it into the output
			CompressMasked( sourceRgba, mask, targetBlock, format, flags, metric );
			
			// advance
			targetBlock += bytesPerBlock;
		}
	}
}
 
 
void Decompress( u8* rgba, void const* block, int format )
{
    // get the block locations
    void const * alphaBock = reinterpret_cast< u8 const* >( block ) + 8;
 
    // decompress colour
    switch( format )
    {
        case kBC1:
            DecompressColour( rgba, block, true);
            break;
        case kBC2:
            DecompressColour( rgba, block, false);
            DecompressAlphaBC2( rgba, alphaBock ); // put in a
            break;
        case kBC3:
            DecompressColour( rgba, block, false);
            DecompressAlphaBC3( rgba, alphaBock, 3 ); // put in a
            break;
 
        case kBC4:
            DecompressAlphaBC3( rgba, block, 0 ); // put in r
            break;
        case kBC5:
            DecompressAlphaBC3( rgba, block, 0 ); // put in r
            DecompressAlphaBC3( rgba, alphaBock, 1 ); // put in g
            break;
    }
}


void DecompressImage( u8* rgba, int width, int height, void const* blocks, int format )
{
	// initialise the block input
	u8 const* sourceBlock = reinterpret_cast< u8 const* >( blocks );
	int bytesPerBlock = GetBlockSize( format );

	// loop over blocks
	for( int y = 0; y < height; y += 4 )
	{
		for( int x = 0; x < width; x += 4 )
		{
			// decompress the block
			u8 targetRgba[4*16];
			Decompress( targetRgba, sourceBlock, format );
			
			// write the decompressed pixels to the correct image locations
			u8 const* sourcePixel = targetRgba;
			for( int py = 0; py < 4; ++py )
			{
				for( int px = 0; px < 4; ++px )
				{
					// get the target location
					int sx = x + px;
					int sy = y + py;
					if( sx < width && sy < height )
					{
						u8* targetPixel = rgba + 4*( width*sy + sx );
						
						// copy the rgba value
						for( int i = 0; i < 4; ++i )
							targetPixel[i] = sourcePixel[i];
					}
					//else
					//{
					//	// skip this pixel as its outside the image
					//
					//}
 
                    sourcePixel += 4;
				}
			}
			
			// advance
			sourceBlock += bytesPerBlock;
		}
	}
}


} // namespace squish
