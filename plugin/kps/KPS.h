
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

#pragma once

#include "PIDefines.h"
#include "PIFormat.h"
#include "PIExport.h"
#include "PIUtilities.h"
#include "PIProperties.h"


// these are format settings for output
enum {
    // lossy BC
    DDS_FMT_BC1 = 0,
    DDS_FMT_BC1S = 1,
	
    DDS_FMT_BC3 = 4,
    DDS_FMT_BC3S = 5,
	
    DDS_FMT_BC4 = 6,
    DDS_FMT_BC4S = 7,
    
    DDS_FMT_BC5 = 8,
    DDS_FMT_BC5S = 9,
    
	DDS_FMT_BC7 = 12,
    DDS_FMT_BC7S = 13,
    
    // lossless formats
    DDS_FMT_R8 = 128,
    DDS_FMT_RG8 = 138,
    DDS_FMT_RGBA8 = 148,
    DDS_FMT_RGBA8S = 158,
    
    DDS_FMT_R16F = 168,
    DDS_FMT_RG16F = 178,
    DDS_FMT_RGBA16F = 188,
    
    DDS_FMT_R32F = 198,
    DDS_FMT_RG32F = 208,
    DDS_FMT_RGBA32F = 218,
    
    // TODO: not sure this should allow lossy export
    // TODO: add ASTC4x4, 5x5, 6x6, 8x8
    // TODO: add ETC2
    
    // TODO: R16S format for depth/heighmaps?, PS can store this for edits
    // PS stores data as 16S acco
};
typedef uint8 DDS_Format;


//-----------------------------

#if 1 // not used

// not used
enum {
	DDS_ALPHA_NONE = 0,
	DDS_ALPHA_TRANSPARENCY,
	DDS_ALPHA_CHANNEL
};
typedef uint8 DDS_Alpha;

// not used
enum{
	DDS_FILTER_BOX,
	DDS_FILTER_TENT,
	DDS_FILTER_LANCZOS4,
	DDS_FILTER_MITCHELL,
	DDS_FILTER_KAISER
};
typedef uint8 DDS_Filter;


// TODO: revisit these options, these are mostly no longer used

// Load options
typedef struct {
	char		sig[4];
	uint8		version;
	DDS_Alpha	alpha;
	uint8		reserved[26];
	
} DDS_inData;

// Save options
typedef struct {
	char			sig[4];
	uint8			version;
	DDS_Format		format;
	DDS_Alpha		alpha;
	Boolean			premultiply;
	Boolean			mipmap;
	DDS_Filter		filter;
	Boolean			cubemap;
	uint8			reserved[245];
	
} DDS_outData;

#else

// no input settings

struct DDS_outData
{
    DDS_Format        format;
    
    // these need UI, should we only do lossless non-mipped import/export
    // kram can do all this, but premul here is lossy
    // much better to script presets for export, and pick from those
    //  the plugin could read the preset file (or embed it).
    //
    //Boolean            premultiply;
    //Boolean            mipmap;
};

#endif


typedef struct Globals
{ // This is our structure that we use to pass globals between routines:

	short				*result;			// Must always be first in Globals.
	FormatRecord		*formatParamBlock;	// Must always be second in Globals.

	//Handle				fileH;				// stores the entire binary file
	
	DDS_inData			in_options;
	DDS_outData			options;
	
} Globals, *GlobalsPtr;



// The routines that are dispatched to from the jump list should all be
// defined as
//		void RoutineName (GPtr globals);
// And this typedef will be used as the type to create a jump list:
typedef void (* FProc)(GlobalsPtr globals);


//-------------------------------------------------------------------------------
//	Globals -- definitions and macros
//-------------------------------------------------------------------------------

#define gResult				(*(globals->result))
#define gStuff				(globals->formatParamBlock)

#define gInOptions			(globals->in_options)
#define gOptions			(globals->options)

#define gAliasHandle		(globals->aliasHandle)

//-------------------------------------------------------------------------------
//	Prototypes
//-------------------------------------------------------------------------------


// Everything comes in and out of PluginMain. It must be first routine in source:
DLLExport MACPASCAL void PluginMain (const short selector,
					  	             FormatRecord *formatParamBlock,
						             intptr_t *data,
						             short *result);

// Scripting functions
Boolean ReadScriptParamsOnWrite (GlobalsPtr globals);	// Read any scripting params.

OSErr WriteScriptParamsOnWrite (GlobalsPtr globals);	// Write any scripting params.

//-------------------------------------------------------------------------------

