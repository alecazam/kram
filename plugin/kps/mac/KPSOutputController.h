
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


#import <Cocoa/Cocoa.h>

#include "KPSUI.h"

typedef enum {
	DIALOG_RESULT_CONTINUE = 0,
	DIALOG_RESULT_OK,
	DIALOG_RESULT_CANCEL
} DialogResult;

@interface KPSOutputController : NSObject {
	IBOutlet NSWindow *theWindow;
	IBOutlet NSPopUpButton *formatPulldown;
	IBOutlet NSButton *mipmapCheck;
	IBOutlet NSPopUpButton *filterPulldown;
	IBOutlet NSTextField *filterLabel;
	IBOutlet NSMatrix *alphaMatrix;
	IBOutlet NSButton *premultiplyCheck;
	IBOutlet NSBox *alphaBox;
	IBOutlet NSButton *cubemapCheck;
	IBOutlet NSButton *ok_button;
	IBOutlet NSButton *cancel_button;
	DialogResult theResult;
}
- (id)init:(DialogFormat)format
	mipmap:(BOOL)mipmap
	filter:(Dialog_Filter)filter
	alpha:(DialogAlpha)alpha
	premultiply:(BOOL)premultiply
	cube_map:(BOOL)cube_map
	have_transparency:(BOOL)has_transparency
	alpha_name:(const char *)alphaName
	ae_ui:(BOOL)ae_ui;

- (IBAction)clickedOK:(id)sender;
- (IBAction)clickedCancel:(id)sender;

- (IBAction)trackMipmap:(id)sender;
- (IBAction)trackAlpha:(id)sender;

- (NSWindow *)getWindow;
- (DialogResult)getResult;

- (DialogFormat)getFormat;
- (BOOL)getMipmap;
- (Dialog_Filter)getFilter;
- (DialogAlpha)getAlpha;
- (BOOL)getPremultiply;
- (BOOL)getCubeMap;

@end
