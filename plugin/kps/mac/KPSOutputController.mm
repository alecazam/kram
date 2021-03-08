
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


#import "KPSOutputController.h"

@implementation KPSOutputController

- (id)init:(DialogFormat)format
	mipmap:(BOOL)mipmap
	filter:(Dialog_Filter)filter
	alpha:(DialogAlpha)alpha
	premultiply:(BOOL)premultiply
	cube_map:(BOOL)cube_map
	have_transparency:(BOOL)has_transparency
	alpha_name:(const char *)alphaName
	ae_ui:(BOOL)ae_ui
{
	self = [super init];
	
    if(!([[NSBundle mainBundle] loadNibNamed:@"KPSOutput" owner:self topLevelObjects:nil]))
		return nil;
	
	// TODO: strings are hardcode in this arrray, these need to line up with
    // actual types now
	[formatPulldown addItemsWithTitles:
	 [NSArray arrayWithObjects:
      @"DXT1",
      @"DXT1A",
      @"DXT2",
      @"DXT3",
      @"DXT4",
      @"DXT5",
      @"DXT5A",
      @"3Dc",
      @"DXN",
      @"Uncompressed",
      nil]
     ];
	[formatPulldown selectItem:[formatPulldown itemAtIndex:format]];
	
	
    [mipmapCheck setState:(mipmap ? NSControlStateValueOn : NSControlStateValueOff)];
	
	
	[filterPulldown addItemsWithTitles:
	 [NSArray arrayWithObjects:@"Box", @"Tent", @"Lanczos4", @"Mitchell", @"Kaiser", nil]];
	[filterPulldown selectItem:[filterPulldown itemAtIndex:filter]];
	
	
	if(!has_transparency)
	{
		[[alphaMatrix cellAtRow:1 column:0] setEnabled:FALSE];
		
		if(alpha == DIALOG_ALPHA_TRANSPARENCY)
		{
			alpha = (alphaName ? DIALOG_ALPHA_CHANNEL : DIALOG_ALPHA_NONE);
		}
	}
	
	if(alphaName)
	{
		[[alphaMatrix cellAtRow:2 column:0] setTitle:[NSString stringWithUTF8String:alphaName]];
	}
	else
	{
		[[alphaMatrix cellAtRow:2 column:0] setEnabled:FALSE];
		
		if(alpha == DIALOG_ALPHA_CHANNEL)
		{
			alpha = (has_transparency ? DIALOG_ALPHA_TRANSPARENCY : DIALOG_ALPHA_NONE);
		}
	}

	[alphaMatrix selectCellAtRow:(NSInteger)alpha column:0];
	
	
    [premultiplyCheck setState:(premultiply ? NSControlStateValueOn : NSControlStateValueOff)];
	
	
    [cubemapCheck setState:(cube_map ? NSControlStateValueOn : NSControlStateValueOff)];


	[self trackMipmap:self];
	[self trackAlpha:self];
	
	if(ae_ui)
	{
		[alphaMatrix setHidden:TRUE];
		[premultiplyCheck setHidden:TRUE];
		[alphaBox setHidden:TRUE];
		
		const int shrink = 170;
		
		NSRect window_frame = [theWindow frame];
		NSRect cube_map_frame = [cubemapCheck frame];
		NSRect ok_frame = [ok_button frame];
		NSRect cancel_frame = [cancel_button frame];
		
		window_frame.size.height -= shrink;
		cube_map_frame.origin.y += shrink;
		ok_frame.origin.y += shrink;
		cancel_frame.origin.y += shrink;
		
		[cubemapCheck setFrame:cube_map_frame];
		[ok_button setFrame:ok_frame];
		[cancel_button setFrame:cancel_frame];
		[theWindow setFrame:window_frame display:TRUE];
	}
	
	[theWindow center];
	
	theResult = DIALOG_RESULT_CONTINUE;
	
	return self;
}

- (IBAction)clickedOK:(id)sender {
	theResult = DIALOG_RESULT_OK;
}

- (IBAction)clickedCancel:(id)sender {
    theResult = DIALOG_RESULT_CANCEL;
}

- (IBAction)trackMipmap:(id)sender {
	const BOOL enabled =  [self getMipmap];
	NSColor *label_color = (enabled ? [NSColor textColor] : [NSColor disabledControlTextColor]);
	
    [filterPulldown setEnabled:enabled];
	[filterLabel setTextColor:label_color];
	
	//[label_color release];
}

- (IBAction)trackAlpha:(id)sender {
	const BOOL enabled = ([self getAlpha] != DIALOG_ALPHA_NONE);
	
	[premultiplyCheck setEnabled:enabled];
}

- (NSWindow *)getWindow {
	return theWindow;
}

- (DialogResult)getResult {
	return theResult;
}

- (DialogFormat)getFormat {
	return  (DialogFormat)[formatPulldown indexOfSelectedItem];
}

- (BOOL)getMipmap {
    return ([mipmapCheck state] == NSControlStateValueOn);
}

- (Dialog_Filter)getFilter {
	return (Dialog_Filter)[filterPulldown indexOfSelectedItem];
}

- (DialogAlpha)getAlpha {
	switch([alphaMatrix selectedRow])
	{
		case 0:		return DIALOG_ALPHA_NONE;
		case 1:		return DIALOG_ALPHA_TRANSPARENCY;
		case 2:		return DIALOG_ALPHA_CHANNEL;
		default:	return DIALOG_ALPHA_CHANNEL;
	}
}

- (BOOL)getPremultiply {
    return ([premultiplyCheck state] == NSControlStateValueOn);
}

- (BOOL)getCubeMap {
    return ([cubemapCheck state] == NSControlStateValueOn);
}

@end
