
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

//-------------------------------------------------------------------------------
//	Definitions -- Required by include files.
//-------------------------------------------------------------------------------

#include "KPSVersion.h"

// TODO: see if can simplify to kram
#define plugInName			"kram"
#define plugInCopyrightYear	DDS_Copyright_Year
#define plugInDescription DDS_Description
#define VersionString 	DDS_Version_String
#define ReleaseString	DDS_Build_Date_Manual
#define CurrentYear		DDS_Build_Year

//-------------------------------------------------------------------------------
//	Definitions -- Required by other resources in this rez file.
//-------------------------------------------------------------------------------

// Dictionary (aete) resources:

#define vendorName			"kram"
#define plugInAETEComment 	DDS_Description

// TODO: bump this sdk higher?
#define plugInSuiteID		'sdK4'
#define plugInClassID		'kram'
#define plugInEventID		typeNull // must be this

//-------------------------------------------------------------------------------
//	Set up included files for Macintosh and Windows.
//-------------------------------------------------------------------------------


#if 0
#include "PIDefines.h"
#else


/// Create a definition if we're on a Macintosh
#ifdef _WIN32
    #define __PIWin__            1
    #define DLLExport extern "C" __declspec(dllexport)

#else
    // this pulls in Carbon.r, but trying not to use Carbon
    // also CoreServices/CoreServices.r is pulled in from another Adobe header, which is also Carbon
    //#include "MacOMacrezXcode.h"

    #define Macintosh    1
    #define MSWindows    0
    #define Rez          1

    #ifndef TARGET_MAC_OS
        #define TARGET_MAC_OS 1
    #endif

    #ifndef DEBUG
        #ifndef NDEBUG
            #define DEBUG 1
        #else
            #define DEBUG 0
        #endif
    #endif

    #define BUILDING_FOR_MACH        1

    // can this carbon dependency be eliminated?
    #ifndef TARGET_API_MAC_CARBON
        #define TARGET_API_MAC_CARBON 1
    #endif

    #include <Carbon.r>

    #define __PIMac__            1
    #define DLLExport extern "C"

    // instead of PIPlatform
    #define PRAGMA_ONCE 1
    #define Macintosh 1
#endif

#if defined(__PIMac__)
	#include "PIGeneral.r"
#elif defined(__PIWin__)
	#include "PIGeneral.h"
#endif

//#include "PIUtilities.r"

#ifndef ResourceID
    #define ResourceID        16000
#endif

#include "PITerminology.h"
#include "PIActions.h"

#include "KPSTerminology.h"

//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

#define USE_KTX  1
#define USE_KTX2 0

resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
		Kind { ImageFormat },
		Name { plugInName },

		//Category { "KTX" },
		//Priority { 1 }, // Can use this to override a built-in Photoshop plug-in

		Version { (latestFormatVersion << 16) | latestFormatSubVersion },

		#ifdef __PIMac__
            #if defined(__arm64__)
                CodeMacARM64 { "PluginMain" },
            #endif
			#if (defined(__x86_64__))
				CodeMacIntel64 { "PluginMain" },
			#endif
		#else
            #if defined(_WIN64)
				CodeWin64X86 { "PluginMain" },
			#endif
            
            // kram-ps not supporting 32-bit PS
			//	CodeWin32X86 { "PluginMain" },
		#endif
	
		// ClassID, eventID, aete ID, uniqueString:
		HasTerminology
        {
            plugInClassID,
            plugInEventID,
            ResourceID,
            vendorName " " plugInName
        },
		
		SupportedModes
		{
			noBitmap,
            noGrayScale, // TODO: add support
			noIndexedColor,
            doesSupportRGBColor, // this is the only supported
			noCMYKColor,
            noHSLColor,
			noHSBColor,
            noMultichannel,
			noDuotone,
            noLABColor
		},
		
        // Using this on macOS to avoid Carbon use.  Uses file descriptor,
        // but doesn't work on Win.  Should really provide FILE* on all platforms.
        SupportsPOSIXIO {},


		EnableInfo { "in (PSHOP_ImageMode, RGBMode, RGBColorMode)" },
	
        // TODO: can't get 'ktx2' extension files to show up
        // tried add thta into list below
        
        #if USE_KTX
        // ktx1 and 2 have the same 4 character start, then ' 1' or ' 2'
		FmtFileType { 'KTX ', '8BIM' },
		ReadTypes { { 'KTX ', '    ' } },
		ReadExtensions { { 'ktx ' } },
		WriteExtensions { { 'ktx ' } },
		FilteredExtensions { { 'ktx ' } },
        #elif USE_KTX2
        FmtFileType { 'KTX2', '8BIM' },
        ReadTypes { { 'KTX2', '    ' } },
        ReadExtensions { { 'ktx2' } },
        WriteExtensions { { 'ktx2' } } // kram can't write KTX2, only read it
        FilteredExtensions { { 'ktx2' } },
        #endif
        
		FormatFlags
        {
            fmtSavesImageResources, //(by saying we do, PS won't store them, thereby avoiding problems)
            fmtCanRead,
            fmtCanWrite,
            fmtCanWriteIfRead,
            fmtCanWriteTransparency,
            fmtCannotCreateThumbnail
        },
        
        // commented these out, so can have larger array textures
		//PlugInMaxSize { 8192, 8192 },
		//FormatMaxSize { { 8192, 8192 } },
        
        // ? shouldn't this be 4, not 5?
		FormatMaxChannels { {   0, 0, 0, 4, 0, 0,
							   0, 0, 0, 0, 0, 0 } },
        
		FormatICCFlags { 	iccCannotEmbedGray,
							iccCannotEmbedIndexed,
							iccCannotEmbedRGB,
							iccCannotEmbedCMYK },
        
        // consider this for reading chunks into layers,
        //   don't need writing can use CopyAllLayers to avoid wait for DoWriteLayer
        // FormatLayerSupport{doesSupportFormatLayers},
        // FormatLayerSupportReadOnly{}
        
    },
};

//-------------------------------------------------------------------------------
//	Dictionary (scripting) resource
//-------------------------------------------------------------------------------

resource 'aete' (ResourceID, plugInName " dictionary", purgeable)
{
	1, 0, english, roman,									/* aete version and language specifiers */
	{
		vendorName,											/* vendor suite name */
		"kram format",							            /* optional description */
		plugInSuiteID,										/* suite ID */
		1,													/* suite code, must be 1 */
		1,													/* suite level, must be 1 */
		{},													/* structure for filters */
		{													/* non-filter plug-in class here */
			"kram",										    /* unique class name */
			plugInClassID,									/* class ID, must be unique or Suite ID */
			plugInAETEComment,								/* optional description */
			{												/* define inheritance */
				"<Inheritance>",							/* must be exactly this */
				keyInherits,								/* must be keyInherits */
				classFormat,								/* parent: Format, Import, Export */
				"parent class format",						/* optional description */
				flagsSingleProperty,						/* if properties, list below */
							
				"Format",
				keyDDSformat,
				typeEnumerated,
				"Output encode format",
				flagsSingleProperty,
				
				// "Alpha Channel",
                // keyDDSalpha,
                // typeEnumerated,
				// "Source of the alpha channel",
				// flagsSingleProperty,
				//
				// "Premultiply",
				// keyDDSpremult,
				// typeBoolean,
				// "Premultiply RGB by Alpha",
				// flagsSingleProperty,
//
				// "Mipmap",
				// keyDDSmipmap,
				// typeBoolean,
				// "Create Mipmaps",
				// flagsSingleProperty,
				//
				// "Filter",
				// keyDDSfilter,
				// typeEnumerated,
				// "Mipmap filter",
				// flagsSingleProperty,
//
				// "Cube Map",
				// keyDDScubemap,
				// typeBoolean,
				// "Convert vertical cross to cube map",
				// flagsSingleProperty,
			},
			{}, /* elements (not supported) */
			/* class descriptions */
		},
		{}, /* comparison ops (not supported) */
		{	/* any enumerations */
			typeDDSformat,
			{
                // explicit
                "E8r",
                formatR8,
                "RGBA8",
                
                "E8rg",
                formatRG8,
                "RGBA8",
                
                "E84",
                formatRGBA8,
                "RGBA8",
                
                "E84S",
                formatRGBA8S,
                "RGBA8 srgb",
                
                "EHr",
                formatR16F,
                "RGBA16F",
                
                "EHrg",
                formatRG16F,
                "RGBA16F",
                
                "EH4",
                formatRGBA16F,
                "RGBA16F",
                
                "EFr",
                formatR32F,
                "RGBA32F",
                
                "EFrg",
                formatRG32F,
                "RGBA32F",
                
                "EF4",
                formatRGBA32F,
                "RGBA32F",
                
                // BC with and without srgb
                "BC1",
                formatBC1,
                "BC1",
                
                "BC3",
                formatBC3,
                "BC3",
                
                "BC4",
                formatBC4,
                "BC4",
                
                "BC5",
                formatBC5,
                "BC5",
                
                "BC7",
                formatBC7,
                "BC7",
                
                
                "BC1S",
                formatBC1S,
                "BC1 srgb",
                
                "BC3S",
                formatBC3S,
                "BC3 srgb",
                
                "BC4S",
                formatBC4S,
                "BC4 srgb",
                
                "BC5S",
                formatBC5S,
                "BC5 srgb",
                
                "BC7S",
                formatBC7S,
                "BC7 srgb"
                
                // TODO: add other formats
			}
			//typeAlphaChannel,
            //{
            //    "None",
            //    alphaChannelNone,
            //    "No alpha channel",

            //    "Transparency",
            //    alphaChannelTransparency,
            //    "Get alpha from Transparency",

            //    "Channel",
            //    alphaChannelChannel,
            //    "Get alpha from channels palette"
            //},
            //typeFilter,
            //{
            //    "Box",
            //    filterBox,
            //    "Box filter",
                
            //    "Tent",
            //    filterTent,
            //    "Tent filter",
            
            //    "Lanczos4",
            //    filterLanczos4,
            //   "Lanczos4 filter",
            
            //   "Mitchell",
            //   filterMitchell,
            //   "Mitchell filter",
            
            //  "Kaiser",
            //  filterKaiser,
            //  "Kaiser filter"
            //}
		}
	}
};


#ifdef __PIMac__

//-------------------------------------------------------------------------------
//	Version 'vers' resources.
//-------------------------------------------------------------------------------

resource 'vers' (1, plugInName " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VersionString,
	VersionString " ©" plugInCopyrightYear " kram"
};

resource 'vers' (2, plugInName " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VersionString,
	"by Alec Miller (based on DDS plugin by Brendan Bolles)"
};


#endif // __PIMac__


