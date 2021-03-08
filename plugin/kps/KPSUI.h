
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

#ifndef DDSUI_H
#define DDSUI_H

typedef enum DialogFormat {
    // bc
	DIALOG_FMT_BC1,
	DIALOG_FMT_BC3,
	DIALOG_FMT_BC4,
	DIALOG_FMT_BC5,
	DIALOG_FMT_BC7,
    
    DIALOG_FMT_BC1S,
    DIALOG_FMT_BC3S,
    DIALOG_FMT_BC4S,
    DIALOG_FMT_BC5S,
    DIALOG_FMT_BC7S,
    
    // TODO: firt decide if lossy formats should be a part of plugin
    //  this encourages opening/saving and each time loss occurs
    // TODO: ETC2
    // TODO: ASTC
    
    // TODO: consider 4.12 Valve type HDR, or 101010A2 or 111110
    
    // lossless formats good for source
    // explicit
    DIALOG_FMT_R8,
    DIALOG_FMT_RG8,
    DIALOG_FMT_RGBA8,
    DIALOG_FMT_RGBA8S,
    
    DIALOG_FMT_R16F,
    DIALOG_FMT_RG16F,
    DIALOG_FMT_RGBA16F,
    
    DIALOG_FMT_R32F,
    DIALOG_FMT_RG32F,
    DIALOG_FMT_RGBA32F,
} DialogFormat;


#if 1 // Not using any of these

typedef enum {
	DIALOG_ALPHA_NONE,
	DIALOG_ALPHA_TRANSPARENCY,
	DIALOG_ALPHA_CHANNEL
} DialogAlpha;

typedef enum {
	DIALOG_FILTER_BOX,
	DIALOG_FILTER_TENT,
	DIALOG_FILTER_LANCZOS4,
	DIALOG_FILTER_MITCHELL,
	DIALOG_FILTER_KAISER
} Dialog_Filter;

typedef struct {
	DialogAlpha		alpha;
} DDS_InUI_Data;

typedef struct {
	DialogFormat		format;
	DialogAlpha			alpha;
	bool				premultiply;
	bool				mipmap;
	Dialog_Filter		filter;
	bool				cubemap;
} DDS_OutUI_Data;

#else
 
// no real input, just want to drop ktx/2 onto PS and go

struct DDS_OutUI_Data {
    DialogFormat        format;
};
#endif

    
// DDS UI
//
// return true if user hit OK
// if user hit OK, params block will have been modified
//
// plugHndl is bundle identifier string on Mac, hInstance on win
// mwnd is the main window for Windows
//
bool
DDS_InUI(
	DDS_InUI_Data		*params,
	bool				has_alpha,
	const void			*plugHndl,
	const void			*mwnd);

bool
DDS_OutUI(
	DDS_OutUI_Data		*params,
	bool				have_transparency,
	const char			*alpha_name,
	bool				ae_ui,
	const void			*plugHndl,
	const void			*mwnd);

void
DDS_About(
	const char		*plugin_version_string,
	const void		*plugHndl,
	const void		*mwnd);
	

// Mac prefs keys
#define DDS_PREFS_ID		"com.ba.kram-ps"
#define DDS_PREFS_ALPHA		"Alpha Mode"
#define DDS_PREFS_AUTO		"Auto"


// Windows registry keys
#define DDS_PREFIX		"Software\\ba\\kram-ps"
#define DDS_ALPHA_KEY	"Alpha"
#define DDS_AUTO_KEY	"Auto"


#endif
