
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

#include "PIDefines.h"
#include "KPS.h"

#include "KPSTerminology.h"
#include "KPSUI.h"
#include "KramImageInfo.h"

using namespace kram;

struct Format {
    DDS_Format fileFormat; // map to MyMTLPixelFormat
    DialogFormat uiFormat; // for UI
    uint32_t signature; // 4 char code
    MyMTLPixelFormat pixelFormat; // this is what kram uses
};

// Note export needs to offer a menu of what format to convert to
// Incoming data is 8u, 16u, or 32f
// Update .r, UI.h file if adding more formats
const Format kFormatTable[] = {
    { DDS_FMT_BC1, DIALOG_FMT_BC1, formatBC1, MyMTLPixelFormatBC1_RGBA },
    { DDS_FMT_BC3, DIALOG_FMT_BC3, formatBC3, MyMTLPixelFormatBC3_RGBA },
    { DDS_FMT_BC4, DIALOG_FMT_BC4, formatBC4, MyMTLPixelFormatBC4_RUnorm },
    { DDS_FMT_BC5, DIALOG_FMT_BC5, formatBC5, MyMTLPixelFormatBC5_RGUnorm },
    { DDS_FMT_BC7, DIALOG_FMT_BC7, formatBC7, MyMTLPixelFormatBC7_RGBAUnorm },
    
    { DDS_FMT_BC1S, DIALOG_FMT_BC1S, formatBC1S, MyMTLPixelFormatBC1_RGBA_sRGB },
    { DDS_FMT_BC3S, DIALOG_FMT_BC3S, formatBC3S, MyMTLPixelFormatBC3_RGBA_sRGB },
    { DDS_FMT_BC4S, DIALOG_FMT_BC4S, formatBC4S, MyMTLPixelFormatBC4_RSnorm },
    { DDS_FMT_BC5S, DIALOG_FMT_BC5S, formatBC5S, MyMTLPixelFormatBC5_RGSnorm },
    { DDS_FMT_BC7S, DIALOG_FMT_BC7S, formatBC7S, MyMTLPixelFormatBC7_RGBAUnorm_sRGB },
    
    // TODO: add ASTC
    // TODO: add ETC2
    
    { DDS_FMT_R8, DIALOG_FMT_R8, formatR8, MyMTLPixelFormatR8Unorm },
    { DDS_FMT_RG8, DIALOG_FMT_RG8, formatRG8, MyMTLPixelFormatRG8Unorm },
    { DDS_FMT_RGBA8, DIALOG_FMT_RGBA8, formatRGBA8, MyMTLPixelFormatRGBA8Unorm },
    { DDS_FMT_RGBA8S, DIALOG_FMT_RGBA8S, formatRGBA8S, MyMTLPixelFormatRGBA8Unorm_sRGB },
    
    { DDS_FMT_R16F, DIALOG_FMT_R16F, formatR16F, MyMTLPixelFormatR16Float },
    { DDS_FMT_RG16F, DIALOG_FMT_RG16F, formatRG16F, MyMTLPixelFormatRG16Float },
    { DDS_FMT_RGBA16F, DIALOG_FMT_RGBA16F, formatRGBA16F, MyMTLPixelFormatRGBA16Float },
    
    { DDS_FMT_R32F, DIALOG_FMT_R32F, formatR32F, MyMTLPixelFormatR32Float },
    { DDS_FMT_RG32F, DIALOG_FMT_RG32F, formatRG32F, MyMTLPixelFormatRG32Float },
    { DDS_FMT_RGBA32F, DIALOG_FMT_RGBA32F, formatRGBA32F, MyMTLPixelFormatRGBA32Float },
};
const int32_t kFormatTableSize = sizeof(kFormatTable) / sizeof(kFormatTable[0]);

static DDS_Format SignatureToFormat(OSType fmt)
{
    for (int32_t i = 0; i < kFormatTableSize; ++i) {
        if (fmt == kFormatTable[i].signature) {
            return kFormatTable[i].fileFormat;
        }
    }
    
    return DDS_FMT_RGBA8;
}

static OSType FormatToSignature(DDS_Format fmt)
{
    for (int32_t i = 0; i < kFormatTableSize; ++i) {
        if (fmt == kFormatTable[i].fileFormat) {
            return kFormatTable[i].signature;
        }
    }

    return formatRGBA8;
}

DialogFormat FormatToDialog(DDS_Format fmt)
{
    for (int32_t i = 0; i < kFormatTableSize; ++i) {
        if (fmt == kFormatTable[i].fileFormat) {
            return kFormatTable[i].uiFormat;
        }
    }

    return DIALOG_FMT_RGBA8;
}

DDS_Format DialogToFormat(DialogFormat fmt)
{
    for (int32_t i = 0; i < kFormatTableSize; ++i) {
        if (fmt == kFormatTable[i].uiFormat) {
            return kFormatTable[i].fileFormat;
        }
    }

    return DIALOG_FMT_RGBA8;
}

MyMTLPixelFormat FormatToPixelFormat(DDS_Format fmt)
{
    for (int32_t i = 0; i < kFormatTableSize; ++i) {
        if (fmt == kFormatTable[i].fileFormat) {
            return kFormatTable[i].pixelFormat;
        }
    }

    return MyMTLPixelFormatRGBA8Unorm;
}
    

//static DDS_Alpha KeyToAlpha(OSType key)
//{
//	return	(key == alphaChannelNone)			? DDS_ALPHA_NONE :
//			(key == alphaChannelTransparency)	? DDS_ALPHA_TRANSPARENCY :
//			(key == alphaChannelChannel)		? DDS_ALPHA_CHANNEL :
//			DDS_ALPHA_TRANSPARENCY;
//}
//
//static DDS_Filter KeyToFilter(OSType key)
//{
//	return	(key == filterBox		? DDS_FILTER_BOX :
//			key == filterTent		? DDS_FILTER_TENT :
//			key == filterLanczos4	? DDS_FILTER_LANCZOS4 :
//			key == filterMitchell	? DDS_FILTER_MITCHELL :
//			key == filterKaiser		? DDS_FILTER_KAISER :
//			DDS_FILTER_MITCHELL);
//}

Boolean ReadScriptParamsOnWrite(GlobalsPtr globals)
{
	PIReadDescriptor			token = NULL;
	DescriptorKeyID				key = 0;
	DescriptorTypeID			type = 0;
	//OSType						shape = 0, create = 0;
	DescriptorKeyIDArray		array = { NULLID };
	int32						flags = 0;
	OSErr						//gotErr = noErr,
    stickyError = noErr;
	Boolean						returnValue = true;
	//int32						storeValue;
	DescriptorEnumID			ostypeStoreValue;
	//Boolean						boolStoreValue;
	
	if (DescriptorAvailable(NULL))
	{
		token = OpenReader(array);
		if (token)
		{
			while (PIGetKey(token, &key, &type, &flags))
			{
				switch (key)
				{
					case keyDDSformat:
							PIGetEnum(token, &ostypeStoreValue);
							gOptions.format = SignatureToFormat(ostypeStoreValue);
							break;
					
//					case keyDDSalpha:
//							PIGetEnum(token, &ostypeStoreValue);
//							gOptions.alpha = KeyToAlpha(ostypeStoreValue);
//							break;
//
//					case keyDDSpremult:
//							PIGetBool(token, &boolStoreValue);
//							gOptions.premultiply = boolStoreValue;
//							break;
//
//					case keyDDSmipmap:
//							PIGetBool(token, &boolStoreValue);
//							gOptions.mipmap = boolStoreValue;
//							break;
//
//					case keyDDSfilter:
//							PIGetEnum(token, &ostypeStoreValue);
//							gOptions.filter = KeyToFilter(ostypeStoreValue);
//							break;
//
//					case keyDDScubemap:
//							PIGetBool(token, &boolStoreValue);
//							gOptions.cubemap = boolStoreValue;
//							break;
				}
			}

			stickyError = CloseReader(&token); // closes & disposes.
				
			if (stickyError)
			{
				if (stickyError == errMissingParameter) // missedParamErr == -1715
					;
					/* (descriptorKeyIDArray != NULL)
					   missing parameter somewhere.  Walk IDarray to find which one. */
				else
					gResult = stickyError;
			}
		}
		
		returnValue = PlayDialog();
		// return TRUE if want to show our Dialog
	}
	
	return returnValue;
}

		

//static OSType AlphaToKey(DDS_Alpha alpha)
//{
//	return	(alpha == DDS_ALPHA_NONE)			? alphaChannelNone :
//			(alpha == DDS_ALPHA_TRANSPARENCY)	? alphaChannelTransparency :
//			(alpha == DDS_ALPHA_CHANNEL)		? alphaChannelChannel :
//			alphaChannelTransparency;
//}
//
//static OSType FilterToKey(DDS_Filter filter)
//{
//	return	(filter == DDS_FILTER_BOX		? filterBox :
//			filter == DDS_FILTER_TENT		? filterTent :
//			filter == DDS_FILTER_LANCZOS4	? filterLanczos4 :
//			filter == DDS_FILTER_MITCHELL	? filterMitchell :
//			filter == DDS_FILTER_KAISER		? filterKaiser :
//			filterMitchell);
//}

OSErr WriteScriptParamsOnWrite(GlobalsPtr globals)
{
	PIWriteDescriptor			token = nil;
	OSErr						gotErr = noErr;
			
	if (DescriptorAvailable(NULL))
	{
		token = OpenWriter();
		if (token)
		{
			// write keys here
			PIPutEnum(token, keyDDSformat, typeDDSformat, FormatToSignature(gOptions.format));

			//PIPutEnum(token, keyDDSalpha, typeAlphaChannel, AlphaToKey(gOptions.alpha));

//			if(gOptions.alpha != DDS_ALPHA_NONE)
//				PIPutBool(token, keyDDSpremult, gOptions.premultiply);
			
//			PIPutBool(token, keyDDSmipmap, gOptions.mipmap);
//
//			if(gOptions.mipmap)
//				PIPutEnum(token, keyDDSfilter, typeFilter, FilterToKey(gOptions.filter));
//
//			PIPutBool(token, keyDDScubemap, gOptions.cubemap);
				
			gotErr = CloseWriter(&token); /* closes and sets dialog optional */
			/* done.  Now pass handle on to Photoshop */
		}
	}
	return gotErr;
}


