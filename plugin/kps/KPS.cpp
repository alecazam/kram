
// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2014, Brendan Bolles
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *	   Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *	   Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
//
// DDS Photoshop plug-in
//
// by Brendan Bolles <brendan@fnordware.com>
//
// ------------------------------------------------------------------------

#include "KPS.h"

#include "KPSVersion.h"
#include "KPSUI.h"

//#include "crn_core.h"
//#include "crn_mipmapped_texture.h"

#include <stdio.h>
#include <assert.h>

//#include <vector>

#ifdef __PIMac__
#include <mach/mach.h>
#endif

#ifndef MIN
#define MIN(A,B)            ( (A) < (B) ? (A) : (B))
#endif
        
#include "Kram.h"
#include "KTXImage.h"
#include "KramImage.h"
#include "KramImageInfo.h"

// this is only on macOS
#include <sys/stat.h>
// including FileUtilities pulls in ObjC crud to .cpp file
//#include "FileUtilities.h"
// these sporadically take intptr_t and int32 on Win, so fix signatures on port
extern OSErr PSSDKWrite(int32 refNum, int32 refFD, int16 usePOSIXIO, int32 * count, void * buffPtr);
extern OSErr PSSDKRead(int32 refNum, int32 refFD, int16 usePOSIXIO, int32 * count, void * buffPtr);
extern OSErr PSSDKSetFPos(int32 refNum, int32 refFD, int16 usePOSIXIO, short posMode, long posOff);

using namespace kram;

// take from KPSScripting.cpp
extern DialogFormat FormatToDialog(DDS_Format fmt);
extern DDS_Format DialogToFormat(DialogFormat fmt);
extern MyMTLPixelFormat FormatToPixelFormat(DDS_Format fmt);


// global needed by a bunch of Photoshop SDK routines
SPBasicSuite *sSPBasic = NULL;

using namespace NAMESPACE_STL;

const char* kBundleIdentifier = "com.ba.kram-ps";

static void DoAbout(AboutRecordPtr aboutP)
{
#ifdef __PIMac__
	const char * const plugHndl = kBundleIdentifier;
	const void *hwnd = aboutP;	
#else
	const HINSTANCE const plugHndl = GetDLLInstance((SPPluginRef)aboutP->plugInRef);
	HWND hwnd = (HWND)((PlatformData *)aboutP->platformData)->hwnd;
#endif

	DDS_About(DDS_Build_Complete_Manual, plugHndl, hwnd);
}


#pragma mark-


static void HandleError(GlobalsPtr globals, const char *errStr)
{
    const int size = MIN(255, strlen(errStr));

    Str255 p_str;
    p_str[0] = size;
    strncpy((char *)&p_str[1], errStr, size);

    PIReportError(p_str);
    gResult = errReportString; // macro uses globals
}

#pragma mark-

static Rect ConvertRect(VRect rect) {
    Rect r;
    r.left = rect.left;
    r.right = rect.right;
    r.top = rect.top;
    r.bottom = rect.bottom;
    return r;
}


#pragma mark-

static void InitGlobals(GlobalsPtr globals)
{	
	// create "globals" as a our struct global pointer so that any
	// macros work:
    //GlobalsPtr globals = (GlobalsPtr)globalPtr;
			
    // load options
	memset(&gInOptions, 0, sizeof(gInOptions));
	strncpy(gInOptions.sig, "Krmi", 4);
	gInOptions.version			= 1;
	gInOptions.alpha			= DDS_ALPHA_CHANNEL; // ignored
	
    // save options
    memset(&gOptions, 0, sizeof(gOptions));
    strncpy(gOptions.sig, "Krmo", 4);
	gOptions.version			= 1;
	gOptions.format				= DDS_FMT_RGBA8;
	gOptions.alpha				= DDS_ALPHA_CHANNEL; // ignored
	gOptions.premultiply		= FALSE; // ignored
	gOptions.mipmap				= FALSE; // ignored
	gOptions.filter				= DDS_FILTER_MITCHELL; // ignored
	gOptions.cubemap			= FALSE;   // ignored
}

// TODO: replace handles with buffers, but revertInfo is a HANDLE, so how to update that?

static Handle myNewHandle(GlobalsPtr globals, const int32 inSize)
{
    return gStuff->handleProcs->newProc(inSize);
}

static Ptr myLockHandle(GlobalsPtr globals, Handle h)
{
    return gStuff->handleProcs->lockProc(h, TRUE);
}

static void myUnlockHandle(GlobalsPtr globals, Handle h)
{
    gStuff->handleProcs->unlockProc(h);
}

static int32 myGetHandleSize(GlobalsPtr globals, Handle h)
{
    return gStuff->handleProcs->getSizeProc(h);
}

static void mySetHandleSize(GlobalsPtr globals, Handle h, const int32 inSize)
{
    gStuff->handleProcs->setSizeProc(h, inSize);
}

// newHandle doesn't have matching call to this
//static void myDisposeHandle(GlobalsPtr globals, Handle h)
//{
//    gStuff->handleProcs->disposeProc(h);
//}


class PSStream
{
public:
    PSStream(int32_t fd);
	virtual ~PSStream() {};

	bool read(void* pBuf, int32_t len);
	bool write(const void* pBuf, int32_t len);
	//bool flush() { return true; };
	uint64_t size();
	//uint64_t tell();
	bool seek(uint64_t ofs);

private:
	int32_t _fd;
};

// posix not supported on Windows, why?


PSStream::PSStream(int32_t fd)
    : _fd(fd)
{
	//seek(0);
}

bool PSStream::read(void* pBuf, int32_t len)
{
    OSErr err = PSSDKRead(0, _fd, (int16_t)true, &len, pBuf);
    return err == 0;
}


bool PSStream::write(const void* pBuf, int32_t len)
{
    OSErr err = PSSDKWrite(0, _fd, (int16_t)true, &len, (void*)pBuf);
    return err == 0;
    
}

// not sure why this isn't a part of the api, and neither is tell?
uint64_t PSStream::size()
{
    struct stat st;
    fstat(_fd, &st);
    return st.st_size;
    
}

bool PSStream::seek(uint64_t offset)
{
    // seek from begnning
    OSErr err = PSSDKSetFPos(0, _fd, (int16_t)true, 1, (long)offset);
    return err == 0;
}


#pragma mark-




// Additional parameter functions
//   These transfer settings to and from gStuff->revertInfo

template <typename T>
static bool ReadParams(GlobalsPtr globals, T *options)
{
	bool found_revert = FALSE;
	
	if ( gStuff->revertInfo != NULL )
	{
		if( myGetHandleSize(globals, gStuff->revertInfo) == sizeof(T) )
		{
			T *flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
			
			memcpy((char*)options, (char*)flat_options, sizeof(T) );
			
			myUnlockHandle(globals, gStuff->revertInfo);
			
			found_revert = TRUE;
		}
	}
	
	return found_revert;
}

template <typename T>
static void WriteParams(GlobalsPtr globals, T *options)
{
	T *flat_options = NULL;
	
	if (gStuff->hostNewHdl != NULL) // we have the handle function
	{
		if (gStuff->revertInfo == NULL)
		{
			gStuff->revertInfo = myNewHandle(globals, sizeof(T) );
		}
		else
		{
			if(myGetHandleSize(globals, gStuff->revertInfo) != sizeof(T)  )
				mySetHandleSize(globals, gStuff->revertInfo, sizeof(T) );
		}
		
		flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
		
		memcpy((char*)flat_options, (char*)options, sizeof(T) );
		
		myUnlockHandle(globals, gStuff->revertInfo);
	}
}


// this is called first on read
static void DoReadPrepare(GlobalsPtr globals)
{
    // posix only on Mac
    if (!gStuff->hostSupportsPOSIXIO)
    {
        //data->gResult = errPlugInHostInsufficient;
        HandleError(globals, "Read - only support posix io");
        return;
    }
    
    // set to indicate posixIO usage
    gStuff->pluginUsingPOSIXIO = TRUE;
    
    
    if (!gStuff->HostSupports32BitCoordinates)
    {
        HandleError(globals, "Read - only support imageSize32");
        return;
    }
    
    // have to ack that plug supports 32-bit
    gStuff->PluginUsing32BitCoordinates = TRUE;
    
	gStuff->maxData = 0;
}

// read first 4 bytes and determine the file system
static void DoFilterFile(GlobalsPtr globals)
{
    // Note: for now only suppor KTX
    //#define DDS_SIG "DDS "

    // note 6 instead of 4 chars
    PSStream stream(gStuff->posixFileDescriptor);
    
    if (!stream.seek(0)) {
        HandleError(globals, "Read - cannot rewind in filter");
        return;
    }
    
    bool isKTX = false;
    bool isKTX2 = false;
    
    uint8_t hdr[6];
    if (stream.read(hdr, kKTXIdentifierSize)) {
        if (memcmp(hdr, kKTXIdentifier, kKTXIdentifierSize) == 0)
            isKTX = true;
        else if (memcmp(hdr, kKTX2Identifier, kKTXIdentifierSize) == 0)
            isKTX2 = true;
    }
    
    // TODO: should this also filter out ktx/ktx2 that are unsupported
    // could mostly look at header except in case of ASTC HDR where ktx
    // must also look for format prop.
    
    if (!(isKTX || isKTX2)) {
        gResult = formatCannotRead;
    }
}


static void DoReadStart(GlobalsPtr globals)
{
    gResult = noErr;
    
    // read it a second time, but only the header
    bool isKTX = false;
    bool isKTX2 = false;
    
    PSStream stream(gStuff->posixFileDescriptor);
    if (!stream.seek(0)) {
        HandleError(globals, "Read - cannot rewind");
        return;
    }

    uint8_t hdr[6];
    if (stream.read(hdr, kKTXIdentifierSize)) {

        if (memcmp(hdr, kKTXIdentifier, kKTXIdentifierSize) == 0)
            isKTX = true;
        else if (memcmp(hdr, kKTX2Identifier, kKTXIdentifierSize) == 0)
            isKTX2 = true;
    }

    
    if (!(isKTX || isKTX2)) {
        HandleError(globals, "Read - no valid ktx/ktx2 signature");
        return;
    }
    
    int32_t w, h;
    MyMTLPixelFormat format;
    KTXHeader header;
    KTX2Header header2;
    
    if (!stream.seek(0)) {
        HandleError(globals, "Read - cannot rewind after sig");
        return;
    }

    if (isKTX) {
        if (!stream.read(&header, sizeof(KTXHeader)))
        {
            HandleError(globals, "Read - couldn't read ktx header");
            return;
        }
        
        w = header.pixelWidth;
        h = header.pixelHeight;
        format = header.metalFormat();
    }
    else {
        if (!stream.read(&header2, sizeof(KTX2Header)))
        {
            HandleError(globals, "Read - couldn't read ktx2 header");
            return;
        }
        
        w = header2.pixelWidth;
        h = header2.pixelHeight;
        format = vulkanToMetalFormat(header2.vkFormat);
    }
    
    gStuff->imageMode = plugInModeRGBColor;
    
    bool hasAlpha = isAlphaFormat(format);
    int32_t numChannels = numChannelsOfFormat(format);
    
    gStuff->imageSize32.h = w;
    gStuff->imageSize32.v = h;

    // plugin sets the numChannels here
    // 3 for rgb, 4 for rgba, ...
    gStuff->planes = numChannels; // (hasAlpha ? 4 : 3);
    
    if (numChannels == 4) {
        bool isPremul = false; // TODO: hookup to premul state in props field (Alb.ra,Alb.ga,...)
        gStuff->transparencyPlane = 3;
        gStuff->transparencyMatting = isPremul ? 1 : 0;
    }
    
    // 16f and 32f go to 32f
    gStuff->depth = isFloatFormat(header.metalFormat()) ? 32 : 8;
    
    
    bool reverting = ReadParams(globals, &gInOptions);
    
    if (!reverting && gStuff->hostSig != 'FXTC')
    {
        DDS_InUI_Data params;
        
    #ifdef __PIMac__
        const char * const plugHndl = kBundleIdentifier;
        const void *hwnd = globals;
    #else
        const HINSTANCE const plugHndl = GetDLLInstance((SPPluginRef)gStuff->plugInRef);
        HWND hwnd = (HWND)((PlatformData *)gStuff->platformData)->hwnd;
    #endif
        
        // DDS_InUI is responsible for not popping a dialog if the user
        // didn't request it.  It still has to set the read settings from preferences though.
        bool result = DDS_InUI(&params, hasAlpha, plugHndl, hwnd);
        
        if(result)
        {
            gInOptions.alpha = params.alpha;
            
            WriteParams(globals, &gInOptions);
        }
        else
        {
            gResult = userCanceledErr;
        }
    }
    
// the following was suppose to set alpha if it was set in the options
// but not using the option anymore.  Honoring the format of the src file.
//    if(gInOptions.alpha == DDS_ALPHA_TRANSPARENCY && gStuff->planes == 4)
}


void CopyImageRectToPS(GlobalsPtr globals, const KTXImage& image, int32_t mipLevel)
{
    // TODO: may need to decocde compressed KTX if want to support those
    int32_t numPlanes = MAX(4, gStuff->planes);
    
    int32_t w = image.width;
    //int32_t h = image.height;
    //int32_t rowBytes = numPlanes * w;
    const uint8_t* pixels = image.fileData + image.mipLevels[mipLevel].offset;
    
    gStuff->data = (void*)pixels;
        
    gStuff->planeBytes = 1;
    gStuff->colBytes = gStuff->planeBytes * numPlanes;
    gStuff->rowBytes = gStuff->colBytes * w;
    
    gStuff->loPlane = 0;
    gStuff->hiPlane = numPlanes - 1;
            
    gStuff->theRect32.left = 0;
    gStuff->theRect32.right = gStuff->imageSize32.h;
    gStuff->theRect32.top = 0;
    gStuff->theRect32.bottom = gStuff->imageSize32.v;
    
    gStuff->theRect = ConvertRect(gStuff->theRect32);
    
    // THis actuall writes the rectangle above from data
    gResult = AdvanceState();
    
    // very important!
    gStuff->data = NULL;
}

static void DoReadContinue(GlobalsPtr globals)
{
    gResult = noErr;
    
    PSStream stream(gStuff->posixFileDescriptor);
    
    if (!stream.seek(0)) {
        HandleError(globals, "Read - cannot rewind after sig");
        return;
    }

    // read it yet a third time, this time reading the first mip
    uint64_t size = stream.size();
    
    // read entire ktx/2 into memory (ideally mmap it)
    vector<uint8_t> data;
    data.resize(size);
    
    if (!stream.read(data.data(), data.size())) {
        HandleError(globals, "Read - Couldn't read file");
        return;
    }
    
    KTXImage srcImage;
    if (!srcImage.open(data.data(), data.size())) { // TODO: consider using isInfoOnly
        HandleError(globals, "Read - Couldn't parse file");
        return;
    }
    
    auto pixelFormat = srcImage.pixelFormat;
    
    KTXImage* outputImage = &srcImage;
    Image imageDecoder;
    KTXImage decodedImage;
    
    if (isExplicitFormat(pixelFormat)) {
        if (isFloatFormat(pixelFormat)) {
            HandleError(globals, "Read - can't decode explicit texture format or type");
            return;
            
            // TODO: not sure that decode does this, code for this exists when a KTX file is imported to kram
            // and then that's converted to an Image to feed to mip gen on the encode side.
//            if (isHalfFormat(pixelFormat)) {
//                TexEncoder decoderType = kTexEncoderUnknown;
//                if (!validateFormatAndDecoder(srcImage.textureType, pixelFormat, decoderType)) {
//                    HandleError(globals, "Read - can't decode this texture format or type");
//                    return;
//                }
//
//                // only need to decode 16f -> 32f, since PS only has 8u, 16u, and 32f
//                TexEncoder decoder = kTexEncoderUnknown;
//                if (!imageDecoder.decode(srcImage, decodedImage, decoder, false, "")) {
//                    HandleError(globals, "Read - Couldn't decode file");
//                    return;
//                }
//                outputImage = &decodedImage;
//            }
        }
    }
    else if (isHdrFormat(pixelFormat)){
        // TODO: hdr block encoded formats must be decoded
        // only ASTC and BC6 formats, but no BC6 support right now
        HandleError(globals, "Read - can't decode hdr texture format or type");
        return;
    }
    else {
        TexEncoder decoder = kTexEncoderUnknown;
        if (!validateFormatAndDecoder(srcImage.textureType, pixelFormat, decoder)) {
            HandleError(globals, "Read - can't decode this texture format or type");
            return;
        }
        
        // ldr block encoded formats must be decoded
        if (!imageDecoder.decode(srcImage, decodedImage, decoder, false, "")) {
            HandleError(globals, "Read - Couldn't decode file");
            return;
        }
        outputImage = &decodedImage;
    }
    
    CopyImageRectToPS(globals, *outputImage, 0);
}




static void DoReadFinish(GlobalsPtr macroUnusedArg(globals))
{

}

#pragma mark-

static void DoOptionsPrepare(GlobalsPtr globals)
{
	gStuff->maxData = 0;
}


static void DoOptionsStart(GlobalsPtr globals)
{
	ReadParams(globals, &gOptions);
	
	if( ReadScriptParamsOnWrite(globals) )
	{
		bool have_transparency = false;
		const char *alpha_name = NULL;
		
        if (gStuff->hostSig == '8BIM')
            // this is a PSD file?
			have_transparency = (gStuff->documentInfo && gStuff->documentInfo->mergedTransparency);
		else
            // either rgba or la
			have_transparency = (gStuff->planes == 2 || gStuff->planes == 4);

			
		if (gStuff->documentInfo && gStuff->documentInfo->alphaChannels)
			alpha_name = gStuff->documentInfo->alphaChannels->name;
	
	
		DDS_OutUI_Data params;
		
		params.format			= FormatToDialog(gOptions.format);
        
		params.alpha			= (DialogAlpha)gOptions.alpha;
		params.premultiply		= gOptions.premultiply;

		params.mipmap			= gOptions.mipmap;

		params.filter			= (gOptions.filter == DDS_FILTER_BOX ? DIALOG_FILTER_BOX :
									gOptions.filter == DDS_FILTER_TENT ? DIALOG_FILTER_TENT :
									gOptions.filter == DDS_FILTER_LANCZOS4 ? DIALOG_FILTER_LANCZOS4 :
									gOptions.filter == DDS_FILTER_MITCHELL ? DIALOG_FILTER_MITCHELL :
									gOptions.filter == DDS_FILTER_KAISER ? DIALOG_FILTER_KAISER :
									DIALOG_FILTER_MITCHELL);
								
		params.cubemap			= gOptions.cubemap;
	
	#ifdef __PIMac__
		const char * const plugHndl = kBundleIdentifier;
		const void *hwnd = globals;	
	#else
		const HINSTANCE const plugHndl = GetDLLInstance((SPPluginRef)gStuff->plugInRef);
		HWND hwnd = (HWND)((PlatformData *)gStuff->platformData)->hwnd;
	#endif

		const bool ae_ui = (gStuff->hostSig == 'FXTC');


		bool result = DDS_OutUI(&params, have_transparency, alpha_name, ae_ui, plugHndl, hwnd);
		
		
		if (result)
		{
            gOptions.format			= DialogToFormat(params.format);

			gOptions.alpha			= params.alpha;
			gOptions.premultiply	= params.premultiply;

			gOptions.mipmap			= params.mipmap;

			gOptions.filter			= (params.filter == DIALOG_FILTER_BOX ? DDS_FILTER_BOX :
										params.filter == DIALOG_FILTER_TENT ? DDS_FILTER_TENT :
										params.filter == DIALOG_FILTER_LANCZOS4 ? DDS_FILTER_LANCZOS4 :
										params.filter == DIALOG_FILTER_MITCHELL ? DDS_FILTER_MITCHELL :
										params.filter == DIALOG_FILTER_KAISER ? DDS_FILTER_KAISER :
										DDS_FILTER_MITCHELL);
										
			gOptions.cubemap		= params.cubemap;
			

			WriteParams(globals, &gOptions);
			WriteScriptParamsOnWrite(globals);
		}
		else
			gResult = userCanceledErr;
	}
}


static void DoOptionsContinue(GlobalsPtr macroUnusedArg(globals))
{

}


static void DoOptionsFinish(GlobalsPtr macroUnusedArg(globals))
{

}

#pragma mark-

// Tis is an esimate of memory use?

static void DoEstimatePrepare(GlobalsPtr globals)
{
    if (!gStuff->HostSupports32BitCoordinates)
    {
        HandleError(globals, "only support imageSize32");
        return;
    }
    
    // poxis only on Mac
    if (!gStuff->hostSupportsPOSIXIO)
    {
        HandleError(globals, "only support posix io");
        return;
    }
    
    // set to indicate posixIO usage
    gStuff->pluginUsingPOSIXIO = TRUE;
    
    // have to ack that plug supports 32-bit
    gStuff->PluginUsing32BitCoordinates = TRUE;
    
	gStuff->maxData = 0;
}


static void DoEstimateStart(GlobalsPtr globals)
{
    int64_t width = gStuff->imageSize32.h;
    int64_t height = gStuff->imageSize32.v;

    // TODO: this assumes single 2d image in dds, and an 8-bit depth multiple
    int64_t numPlanes = MAX(4, gStuff->planes);
    int64_t depth = gStuff->depth; // this is in bits
   
	int64_t dataBytes = (width * height * numPlanes * depth + 7) >> 3;
			
    // this is how much space we need to write out data as KTX/KTX2 file
    // KTX can precompute max size from width/height and mip count, but ktx2 is compressed
    // I think PS will make sure there are enough
    
    // Can we get this number quickly out of kram based on mip setting, format, etc.
    // May not always write out full depth.  May have encoded format.
    
    size_t bytesToRead = dataBytes;
    
	gStuff->minDataBytes = MIN(bytesToRead  / 2, INT_MAX);
	gStuff->maxDataBytes = MIN(bytesToRead, INT_MAX);
	
	gStuff->data = NULL;
}


static void DoEstimateContinue(GlobalsPtr macroUnusedArg(globals))
{

}


static void DoEstimateFinish(GlobalsPtr macroUnusedArg(globals))
{

}

#pragma mark-

static void DoWritePrepare(GlobalsPtr globals)
{
    if (!gStuff->HostSupports32BitCoordinates)
    {
        HandleError(globals, "only support imageSize32");
        return;
    }
    
    // poxis only on Mac
    if (!gStuff->hostSupportsPOSIXIO)
    {
        HandleError(globals, "only support posix io");
        return;
    }
    
    // set to indicate posixIO usage
    gStuff->pluginUsingPOSIXIO = TRUE;
    
    // have to ack that plug supports 32-bit
    gStuff->PluginUsing32BitCoordinates = TRUE;
    
	gStuff->maxData = 0;
}

// TODO: extent to take a rect, and return he data
static bool CopyImageRectFromPS(GlobalsPtr globals, vector<uint8_t>&pixels, int32_t numPlanes, int32_t width, int32_t height)
{
    int32_t rowBytes = width * numPlanes;
    
    // this is where data will go
    pixels.resize(rowBytes * height);
    
    gStuff->loPlane = 0;
    gStuff->hiPlane = numPlanes-1; // either b for rgb, or a for rgba are the last byte
    gStuff->planeBytes = sizeof(unsigned char); // 1 for interleaved data
    gStuff->colBytes = gStuff->planeBytes * numPlanes;
    gStuff->rowBytes = rowBytes; // * gStuff->colBytes; // interleaved or non-interleaved data is why colBytes is here
    
    gStuff->theRect32.left = 0;
    gStuff->theRect32.right = gStuff->theRect32.left + width;
    gStuff->theRect32.top = 0;
    gStuff->theRect32.bottom = gStuff->theRect32.top + height;

    // set deprecated rect
    gStuff->theRect = ConvertRect(gStuff->theRect32);
    
    // This fills out the pixel data
    gStuff->data = pixels.data();
    
    gResult = AdvanceState();
    if (gResult != noErr) {
        HandleError(globals, "Write - AdvanceState failed to read pixels");
    }
    
    // this pack alpha into 3rd channel?
    bool have_alpha_channel = (gStuff->channelPortProcs && gStuff->documentInfo && gStuff->documentInfo->alphaChannels);
    if (gResult == noErr && have_alpha_channel) //  && gOptions.alpha == DDS_ALPHA_CHANNEL)
    {
        ReadPixelsProc ReadProc = gStuff->channelPortProcs->readPixelsProc;
        
        ReadChannelDesc *alpha_channel = gStuff->documentInfo->alphaChannels;

        VRect wroteRect;
        VRect writeRect = { 0, 0, height, width }; // tlbr
    
        PSScaling scaling;
        scaling.sourceRect = writeRect;
        scaling.destinationRect = writeRect;
    
        // this is converting to bits
        PixelMemoryDesc memDesc = { (char *)gStuff->data, gStuff->rowBytes * 8, gStuff->colBytes * 8, 3 * 8, gStuff->depth };
    
        gResult = ReadProc(alpha_channel->port, &scaling, &writeRect, &memDesc, &wroteRect);
        
        if (gResult != noErr) {
            HandleError(globals, "Write - convert layer to 4 channels failed");
        }
    }
    
    // very important!, so it's not filled in with data again and again on remaining AdvanceState calls
    gStuff->data = NULL;
    
    if (gResult != noErr) {
        return false;
    }
    
    return true;
}

static void DoWriteStart(GlobalsPtr globals)
{
	ReadParams(globals, &gOptions);
	ReadScriptParamsOnWrite(globals);

    // Xcode can't debug p gStuff->..., so must type p global->formatParamBlock->...
    
    int32_t numPlanes = MAX(4, gStuff->planes);

    if (gStuff->imageMode != plugInModeRGBColor) {
        HandleError(globals, "Not rgb color");
        return;
    }
    
    if (gStuff->depth != 8) {
        HandleError(globals, "Not 8-bit color");
        return;
    }
    
    // Note: loadImageFromPixels only works with 4 byte image right now
    bool haveAlpha = (numPlanes == 4) || ((gStuff->channelPortProcs && gStuff->documentInfo && gStuff->documentInfo->alphaChannels));
    if ((numPlanes != 4) || (gStuff->planes == 3 && !haveAlpha))
    {
        HandleError(globals, "Not 4 planes, or 3 with alpha");
        return;
    }
	
    int width = gStuff->imageSize32.h;
    int height = gStuff->imageSize32.v;
	
    // this is a potentiall large memory allocation for one level of the image
    vector<uint8_t> pixels;
    if (!CopyImageRectFromPS(globals, pixels, numPlanes, width, height)) {
        //return;
    }
    
    Image srcImage;
    ImageInfo dstImageInfo;
    KTXImage dstImage;
    
    if (gResult == noErr)
    {
        // convert pixels into ktx with mips if needed in memory
        // note: cannot roundtrip mips, so may want to not do mips or block encodes here
        // try to support
        // TODO: this is limiting since loadImage must be single 2D image
        
        if (!srcImage.loadImageFromPixels(pixels, width, height, true, true)) {
            HandleError(globals, "Write - loadImageFromPixels failed");
        }
    }

    if (gResult == noErr)
    {
        MyMTLPixelFormat pixelFormat = FormatToPixelFormat(gOptions.format);
        
        // setup all the data to generate dstImage
        // now apply user picked format
        ImageInfoArgs dstImageInfoArgs;
        dstImageInfoArgs.pixelFormat = pixelFormat;
        dstImageInfoArgs.doMipmaps = false;
        
        // ?
        // photoshop provides raw image as unmultiplied, but need to premul it
//        if (haveAlpha) {
//            dstImageInfoArgs.isPremultiplied = true;
//        }
        
        if (!validateFormatAndEncoder(dstImageInfoArgs)) {
            HandleError(globals, "Write - validate format failed");
        }
        else {
            dstImageInfo.initWithArgs(dstImageInfoArgs);
           
            if (!srcImage.encode(dstImageInfo, dstImage)) {
                HandleError(globals, "Write - encode failed");
            }
        }
    }
    
    // testing only
    //HandleError(globals, "Write - made it past encode");
    
    if (gResult == noErr) {
        // this needs to write ktx with mips and all to the memory, then copy it to dataFork
        // is this dataFork even valid anymore
        PSStream stream(gStuff->posixFileDescriptor); // write
     
        // TOOD: this is writing 1k x 1k image out as 8MB instead of 4MB
        // see if validate above fixes that.
        
        if (!stream.write(dstImage.fileData, (int32_t)dstImage.fileDataLength)) {
            HandleError(globals, "Write - stream write failed");
        }
    }
}


static void DoWriteContinue(GlobalsPtr macroUnusedArg(globals))
{

}


static void DoWriteFinish(GlobalsPtr globals)
{
	if (gStuff->hostSig != 'FXTC')
		WriteScriptParamsOnWrite(globals);
}


#pragma mark-


DLLExport MACPASCAL void PluginMain(const short selector,
						             FormatRecord *formatParamBlock,
                                     intptr_t *dataPointer,
						             short *result)
{
    // using this to keep dead-strip from removing all code
    if (selector == formatSelectorAbout && formatParamBlock == nullptr)
    {
        return;
    }
    
	if (selector == formatSelectorAbout)
	{
		sSPBasic = ((AboutRecordPtr)formatParamBlock)->sSPBasic;

		DoAbout((AboutRecordPtr)formatParamBlock);
	}
	else
	{
		sSPBasic = formatParamBlock->sSPBasic;  //thanks Tom
            
        GlobalsPtr globals = (GlobalsPtr)*dataPointer;
        if (globals == NULL)
        {
            globals = (GlobalsPtr)malloc(sizeof(Globals));
    
            if(globals == NULL) {
                *result = memFullErr;
                return;
            }
            
            InitGlobals(globals);
            
            *dataPointer = (intptr_t)globals;
        }

        globals->result = result;
        globals->formatParamBlock = formatParamBlock;

#if 1
        static const FProc routineForSelector [] =
        {
            /* formatSelectorAbout                  DoAbout, */

            /* formatSelectorReadPrepare */            DoReadPrepare,
            /* formatSelectorReadStart */            DoReadStart,
            /* formatSelectorReadContinue */        DoReadContinue,
            /* formatSelectorReadFinish */            DoReadFinish,

            /* formatSelectorOptionsPrepare */        DoOptionsPrepare,
            /* formatSelectorOptionsStart */        DoOptionsStart,
            /* formatSelectorOptionsContinue */        DoOptionsContinue,
            /* formatSelectorOptionsFinish */        DoOptionsFinish,

            /* formatSelectorEstimatePrepare */        DoEstimatePrepare,
            /* formatSelectorEstimateStart */        DoEstimateStart,
            /* formatSelectorEstimateContinue */    DoEstimateContinue,
            /* formatSelectorEstimateFinish */        DoEstimateFinish,

            /* formatSelectorWritePrepare */        DoWritePrepare,
            /* formatSelectorWriteStart */            DoWriteStart,
            /* formatSelectorWriteContinue */        DoWriteContinue,
            /* formatSelectorWriteFinish */            DoWriteFinish,

            /* formatSelectorFilterFile */            DoFilterFile
        };

        // Dispatch selector
        if (selector > formatSelectorAbout && selector <= formatSelectorFilterFile)
            (routineForSelector[selector-1])(globals); // dispatch using jump table
        else
            gResult = formatBadParameters;
        
#else
        // This explicit dispatch is much easier to follow
        // can can set breakpoints, and step from a central dispatch point here.
        
        // Dispatch selector.
        switch (selector) {
            case formatSelectorReadPrepare:
                DoReadPrepare(format_record, data, result);
                break;
            case formatSelectorReadStart:
                DoReadStart(format_record, data, result);
                break;
            case formatSelectorReadContinue:
                DoReadContinue(format_record, data, result);
                break;
            case formatSelectorReadFinish:
                DoReadFinish(format_record, data, result);
                break;

            case formatSelectorOptionsPrepare:
                DoOptionsPrepare(format_record, data, result);
                break;
            case formatSelectorOptionsStart:
                DoOptionsStart(format_record, data, result, plugin_ref);
                break;
            case formatSelectorOptionsContinue:
                DoOptionsContinue(format_record, data, result);
                break;
            case formatSelectorOptionsFinish:
                DoOptionsFinish(format_record, data, result);
                break;

            case formatSelectorEstimatePrepare:
                DoEstimatePrepare(format_record, data, result);
                break;
            case formatSelectorEstimateStart:
                DoEstimateStart(format_record, data, result);
                break;
            case formatSelectorEstimateContinue:
                DoEstimateContinue(format_record, data, result);
                break;
            case formatSelectorEstimateFinish:
                DoEstimateFinish(format_record, data, result);
                break;

            case formatSelectorWritePrepare:
                DoWritePrepare(format_record, data, result);
                break;
            case formatSelectorWriteStart:
                DoWriteStart(format_record, data, result);
                break;
            case formatSelectorWriteContinue:
                DoWriteContinue(format_record, data, result);
                break;
            case formatSelectorWriteFinish:
                DoWriteFinish(format_record, data, result);
                break;

            case formatSelectorReadLayerStart:
                DoReadLayerStart(format_record, data, result);
                break;
            case formatSelectorReadLayerContinue:
                DoReadLayerContinue(format_record, data, result);
                break;
            case formatSelectorReadLayerFinish:
                DoReadLayerFinish(format_record, data, result);
                break;

            case formatSelectorWriteLayerStart:
                DoWriteLayerStart(format_record, data, result);
                break;
            case formatSelectorWriteLayerContinue:
                DoWriteLayerContinue(format_record, data, result);
                break;
            case formatSelectorWriteLayerFinish:
                DoWriteLayerFinish(format_record, data, result);
                break;

            case formatSelectorFilterFile:
                DoFilterFile(format_record, data, result);
                break;
            }
        }
#endif
    
	}
}

// This is just to silence broken build.
// Even though this is a plugin, Xcode wants _main or won't link.
int main(int macroUnusedArg(argc), char** macroUnusedArg(argv))
{
    // call this to prevent dead-stripping
    PluginMain(formatSelectorAbout, nullptr, nullptr, nullptr);
    
    return 0;
}
