/*
The MIT License (MIT)

Copyright (c) 2015 Philip Rideout

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "hedistance.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

// See here for more info:
// https://prideout.net/coordinate-fields
// https://prideout.net/blog/distance_fields/
//
// using sedt (squared euclidian distance transform), then can store distances as integer values
// then can convert sedt to edt at the end.
// SDF = edt - invert(edt)
//
// This implements marching parabola's algorithm found here:
// http://cs.brown.edu/people/pfelzens/dt/
//
// One nice property of this algorithm is that the computed distance field need not have the same resolution as the
// source data.  Mip values are computed by sampling the parabolic functions.
//
// For gpu impl, look into jump flooding with the min erosion algorithm.
// https://dl.acm.org/doi/abs/10.1145/1111411.1111431
// Jump flood was good if you didn't have compute, but maybe better just to convert edt to compute.
// Since it's O(n) and can run everything in one pass.  And have R32f targets and gather ops.
//
//
// also can check out this
// https://github.com/giorgiomarcias/distance_transform
//
// CPCF is useful for nice contouring of meshes, see the paper here.
// It's only slightly more storage than SDF, but you can extract more from it.
// But it looks like this code only gens an df from it.
// https://www.in.tum.de/fileadmin/w00bws/cg/Research/Publications/2013/Closest_Point_Contouring/closest_point_contouring__eg_shortpaper.pdf

// TODO: To handle wrapping, need to repeat image, then extract the center result
// so 3x1 or 1x3 for one axis or a full 3x3 areas for both axes.

// On handling SDF and text
// https://www.oreilly.com/library/view/iphone-3d-programming/9781449388133/ch07.html

// For sdf shader eval, use 0.7 * length(ddx(d) + ddy(d))
// instead of fwidth is abs(ddx(d)) + abs(ddy(d)) it's just an approx.

// Note: this can sample from a much higher resolution image, since the distances
// are evaluated off parabolas.  So the dst >= src pixels.

// I think despite the speed of this algorithm, the need for super high-resolution bitmaps
// points to using something more like analytic tracing of inside/outside shapes composed of paths.
// That's what ARM did here:
//
// Practical Analytic 2D Signed Distance Field Generation (Siggraph 2016). Also in Papers.
// https://dl.acm.org/doi/pdf/10.1145/2897839.2927417
// Although strokes aren't so easily cracked into quadratics.   So examples are mostly fills.
//  Strokes are like 5th order curves with various endpoint types.
// But they integrated it into Skia, and pointed to impl of only their anaylytic formulas
// https://www.desmos.com/calculator/wxqfhychxu

namespace heman {
    
struct heman_image {
    int32_t width;
    int32_t height;
    int32_t nbands;
    float* data;
};

static heman_image* heman_image_create(int32_t width, int32_t height, int32_t nbands)
{
    heman_image* img = (heman_image*)malloc(sizeof(heman_image));
    img->width = width;
    img->height = height;
    img->nbands = nbands;
    img->data = (float*)malloc(sizeof(float) * width * height * nbands);
    return img;
}

static void heman_image_destroy(heman_image* img)
{
    free(img->data);
    free(img);
}


//---------------
// defines for image

// An "image" encapsulates three integers (width, height, number of bands)
// and an array of (w * h * nbands) floats, in scanline order.  For simplicity
// the API disallows struct definitions, so this is just an opaque handle.

// 1E20 isn't big enough to process a 2k image with 1 pixel in each corner
const float INF = 1E20;
// 2k max image is 11-bits x squared = 22 + 1
// this is also the limit of single-float precision in the mantissa
//const float INF = 1E23;

#define NEW(t, n) (t*)calloc(n, sizeof(t))
#define SQR(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// @ 8k x 8K resolution, this needs 1/2 GB for the fp32 buffers
// so it resizes to the dst area.  It doesn't re-eval off parabolas until the very end.
// and needs first pass data for second pass.

// This is really the sedt (squared euclidian distance transform).
// The advantage is this can be stored in integer values, but this does parabolic lookup.
// Also since EDT is done as a separable filter, it completes very quickly in O(N) of two passes.
static void squared_edt(
    const float* f, float* d, float* z, int32_t* w, int32_t numSrcSamples, int32_t numDstSamples)
{
    // hull vertices
    w[0] = 0;
    
    // hull intersections
    z[0] = -INF;
    z[1] = +INF;
    
    for (int32_t k = 0, q = 1; q < numSrcSamples; ++q) {
        int32_t wk = w[k];
        float s;
        
        s = ((f[q] - f[wk]) + (float)(SQR(q) - SQR(wk))) / (float)(2 * (q - wk));
        
        // this additional parabolic search completes in 0 or 1 iterations, so algorithm still O(n)
        // sarch back and replace any higher parabola
        while (s <= z[k]) {
            --k;
            wk = w[k];
            
            s = ((f[q] - f[wk]) + (float)(SQR(q) - SQR(wk))) / (float)(2 * (q - wk));
        }
        
        k++;
        
        // store index of lowest points?
        w[k] = q;
        
        // store intersection
        z[k] = s;
        z[k + 1] = +INF;
    }
    
    // Note: this can resample do a different sample count, since the stored parabolas
    // can be evaluated at any point along the curve.
    bool isResampling = numSrcSamples > numDstSamples;
    float conversion = (numSrcSamples / (float)numDstSamples);
    
    for (int32_t k = 0, q = 0; q < numDstSamples; ++q) {
        float qSrc = (float)q;
        // convert q in dstSamples into sample in srcSamples
        if (isResampling) {
            qSrc *= conversion;
        }
        
        // lookup the parabola, and evalute distance squared from that
        while (z[k + 1] < qSrc) {
            ++k;
        }
        int32_t wk = w[k];
        d[q] = f[wk] + (float)SQR(qSrc - (float)wk);
        
        // above is adding intersection height to existing sample
        // of the lowest point intersection
    }
}

// this runs a vertical sedt, then a horizontal sedt, then to edt
// downsamples in the process
static void transform_to_distance(heman_image* temp, const my_image* src, int32_t dstHeight, bool isPositive)
{
    int32_t srcWidth = src->width;
    int32_t srcHeight = src->height;
    
    int32_t dstWidth = temp->width;
    
    assert(srcWidth >= dstWidth && srcHeight >= dstHeight);
    
    // these can all just be strip buffers per thread, but only one thread
    // these were originall turned into 2d arrays for omp
    int32_t maxDim = MAX(srcWidth, srcHeight);
    float* f = NEW(float, maxDim);
    float* d = NEW(float, maxDim);
    float* z = NEW(float, maxDim+1); // padded by 1
    int32_t* w = NEW(int32_t, maxDim);

    // process rows
    for (int32_t y = 0; y < srcHeight; ++y) {
        const uint8_t* s = src->data + y * srcWidth;
        
        // load data into the rows, this is because tmp width is dstWidth, not srcWidth
        if (isPositive) {
            for (int32_t x = 0; x < srcWidth; ++x) {
                f[x] = s[x] ? INF : 0;
            }
        }
        else {
            for (int32_t x = 0; x < srcWidth; ++x) {
                f[x] = s[x] ? 0 : INF;
            }
        }
        
        // downsample the incoming data into fewer columns
        // this is only pulling from closest parabola, not bilerping
        squared_edt(f, d, z, w, srcWidth, dstWidth);
        
        float* t = temp->data + y * dstWidth;
        for (int32_t x = 0; x < dstWidth; ++x) {
            t[x] = d[x];
        }
    }
    
    // process columns
    for (int32_t x = 0; x < dstWidth; ++x) {
        float* t = temp->data + x;
        
        for (int32_t y = 0; y < srcHeight; ++y) {
            f[y] = t[y * dstWidth];
        }
        
        // downsample the incoming data into fewer rows
        // this is only pulling from closest parabola, not bilerping
        squared_edt(f, d, z, w, srcHeight, dstHeight);
        
        for (int32_t y = 0; y < dstHeight; ++y) {
            t[y * dstWidth] = sqrtf(d[y]); // back to distance
        }
    }
    
    free(f);
    free(d);
    free(z);
    free(w);
}

// returns 1-channel float texture with SDF, is it already -1 to 1 ?
void heman_distance_create_sdf(const my_image* src, my_image* dst, float& maxD, bool isVerbose)
{
    // Would have to fix transform_to_distance to use nbands to remove this constraint
    assert(src->numChannels == 1 && "Distance field input must have only 1 band.");
    
    //assert(src->width == dst->width && src->height == dst->height);
    
    int32_t dstWidth = dst->width;
    int32_t dstHeight = dst->height;
    
    // This can downsample a much larger bitmap, but requires two big float buffers.
    // Horizontal is downsampled first, so w > h is ideal, but most incoming textures are square pow2.
    // Temp data - one with positive edt, the other negative edt.
    // 8k x 8k downsmpled to 1k x 1k only needs a 1k x 8k buffer temp buffer for 32MB.
    // But the full 8k x 8k bitmap/grayscale texture is still walked which is 64MB @ 8bits, or 8MB at 1-bit.
    heman_image* positive = heman_image_create(dstWidth, src->height, 1);
    heman_image* negative = heman_image_create(dstWidth, src->height, 1);
   
    // compute edt
    // buffers need to hold dstWidth * srcHeight, and after hold dstWidth * dstHeight
    transform_to_distance(positive, src, dstHeight, true);
    transform_to_distance(negative, src, dstHeight, false);
    
    if (maxD == 0) {
        // now find signed distance, and store back into positive array
        float minV = 0.0f, maxV = 0.0f;
        
        for (int32_t y = 0; y < dstHeight; ++y) {
            const float* pp = positive->data + y * dstWidth;
            const float* nn = negative->data + y * dstWidth;
           
            for (int32_t x = 0; x < dstWidth; ++x) {
                float v = pp[x] - nn[x];
                if (v > maxV) {
                    maxV = v;
                }
                if (v < minV) {
                    minV = v;
                }
            }
        }
        
        // want to scale all mips the same, so only update maxD for the first mip level
        float absMaxV = ceilf(MAX(fabs(minV), maxV));
        int32_t scaling = 0; // clamp to maxD
        
        maxD = 127.0f;
        
        // scale up values if distances are small
        // could have a range below 127 where don't upscale
        // and range slightly above where downscale
        
        if (absMaxV <= maxD) {
            maxD = absMaxV;
        }
        
        // this clamps if absMaxV > maxD to avoid large empty spaces blowing out precision
        
        if (absMaxV <= maxD) {
            scaling = 1; // upscale
        }
        
        // Not sure whether or how to scale edt based on image dimensions.
        // Could divide by max(maxP,maxN) to normalize up/down, but large empty regions of texture blow out
        // the final precision.  Clamping and not normalizing small textures also wastes precision.
        bool debug = isVerbose;
        if (debug) {
            KLOGI( "Heman", "mipLevel %dx%d is %s\n"
                    "min/maxV = %.2f, %.2f\n"
                    "absMaxV = %.2f\n"
                    "maxD = %.2f\n",
                    dstWidth, dstHeight,
                    scaling == 0 ? "clamped" : "scaled", minV, maxV, absMaxV, maxD);
        }
    }
    
    
    
    float invMaxD = 1.0/maxD;

    // now know the range, can normalize or clamp accordingly
    // TODO: should this normalize, since distance depends on empty area in texture
    // and also smaller textures don't have as big of distances.  Want to scale to full -1 to 1 range.
    // pixel distances on 64 vs 2k texture are proportional to the size of the texture and empty space.
    // How does downsampling affect all this, since distances computed from larger texture parabolas.

    // how to scale the SDF from big size down to smaller mips.
    // the distances of downsampled are scaled into largest size mip
    // so any scaling should apply to them all.
    
    for (int32_t y = 0; y < dstHeight; ++y) {
        uint8_t* d = dst->data + y * dstWidth;
        const float* pp = positive->data + y * dstWidth;
        const float* nn = negative->data + y * dstWidth;
       
        for (int32_t x = 0; x < dstWidth; ++x) {
            float v = pp[x] - nn[x];
           
            if (v > maxD) {
                v = 1.0f;
            }
            else if (v < -maxD) {
                v = -1.0f;
            }
            else {
                // this may gen approx 1, since it's not a divide
                v *= invMaxD;
            }
            
            // this is snorm definition 0 = 128, so in unorm 128.0/255.0 is the 0 point
            d[x] = (uint8_t)(roundf(v * 127.0f) + 128.0f);
        }
    }
    
    heman_image_destroy(positive);
    heman_image_destroy(negative);
}


}
