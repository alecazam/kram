
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

// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

// ------------------------------------------------------------------------
//
// DDS Photoshop plug-in
//
// by Brendan Bolles <brendan@fnordware.com>
//
// ------------------------------------------------------------------------

#include "KPSUI.h"

#import "KPSInputController.h"
#import "KPSOutputController.h"
#import "KPSAboutController.h"

#include "KPSVersion.h"

#include "PIUtilities.h"


bool
DDS_InUI(
	DDS_InUI_Data		*params,
	bool				has_alpha,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = true;
	
	params->alpha = DIALOG_ALPHA_CHANNEL;
	
	// get the prefs
	BOOL auto_dialog = FALSE;
	
	CFPropertyListRef alphaMode_val = CFPreferencesCopyAppValue(CFSTR(DDS_PREFS_ALPHA), CFSTR(DDS_PREFS_ID));
	CFPropertyListRef auto_val = CFPreferencesCopyAppValue(CFSTR(DDS_PREFS_AUTO), CFSTR(DDS_PREFS_ID));

	if(alphaMode_val)
	{
		char alphaMode_char;
		
		if( CFNumberGetValue((CFNumberRef)alphaMode_val, kCFNumberCharType, &alphaMode_char) )
		{
			params->alpha = (DialogAlpha)alphaMode_char;
		}
		
		CFRelease(alphaMode_val);
	}

	if(auto_val)
	{
		auto_dialog = CFBooleanGetValue((CFBooleanRef)auto_val);
		
		CFRelease(auto_val);
	}
	
	
	// user can force dialog open buy holding shift or option
	const NSUInteger flags = [[NSApp currentEvent] modifierFlags];
    const bool shift_key = ( (flags & NSEventModifierFlagShift) || (flags & NSEventModifierFlagOption) );

	if((has_alpha && auto_dialog) || shift_key)
	{
		// do the dialog (or maybe not (but we still load the object to get the prefs)
		NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

		Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
										classNamed:@"KPSInputController"];

		if(ui_controller_class)
		{
			KPSInputController *ui_controller = [[ui_controller_class alloc] init:params->alpha
														autoDialog:auto_dialog];
			
			if(ui_controller)
			{
				NSWindow *my_window = [ui_controller getWindow];
				
				if(my_window)
				{
					NSInteger modal_result = [NSApp runModalForWindow:my_window];
					
					if(modal_result == NSModalResponseStop)
					{
						params->alpha = [ui_controller getAlpha];
						
						result = true;
					}
					else
						result = false;
					
					
					// record the auto pref every time
					CFBooleanRef autoRef =  ([ui_controller getAuto] ? kCFBooleanTrue : kCFBooleanFalse);
					CFPreferencesSetAppValue(CFSTR(DDS_PREFS_AUTO), autoRef, CFSTR(DDS_PREFS_ID));
					
					CFPreferencesAppSynchronize(CFSTR(DDS_PREFS_ID));
										
					
					[my_window close];
				}

				//[ui_controller release];
			}
		}
	}


	return result;
}


bool
DDS_OutUI(
	DDS_OutUI_Data		*params,
	bool				have_transparency,
	const char			*alpha_name,
    bool				ae_ui,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = true;

	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

	Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"KPSOutputController"];

	if(ui_controller_class)
	{
		KPSOutputController *ui_controller = [[ui_controller_class alloc] init: params->format
																		  mipmap: params->mipmap
																		  filter: params->filter
																		   alpha: params->alpha
																	 premultiply: params->premultiply
																		cube_map: params->cubemap
															   have_transparency: have_transparency
																	  alpha_name: alpha_name
																		   ae_ui: ae_ui ];

		if(ui_controller)
		{
			NSWindow *my_window = [ui_controller getWindow];
			
			if(my_window)
			{
				NSInteger modal_result;
				DialogResult dialog_result;
				
				NSModalSession modal_session = [NSApp beginModalSessionForWindow:my_window];
				
				do{
					modal_result = [NSApp runModalSession:modal_session];

					dialog_result = [ui_controller getResult];
				}
				while(dialog_result == DIALOG_RESULT_CONTINUE && modal_result == NSModalResponseContinue);
				
				[NSApp endModalSession:modal_session];
				
				
				if(dialog_result == DIALOG_RESULT_OK || modal_result == NSModalResponseStop)
				{
					params->format			= [ui_controller getFormat];
					params->mipmap			= [ui_controller getMipmap];
					params->filter			= [ui_controller getFilter];
					params->alpha			= [ui_controller getAlpha];
					params->premultiply		= [ui_controller getPremultiply];
					params->cubemap			= [ui_controller getCubeMap];
					
					result = true;
				}
				else
					result = false;
					
				[my_window close];
			}
			
			//[ui_controller release];
		}
	}
	
	
	return result;
}


void
DDS_About(
	const char		*plugin_version_string,
	const void		*plugHndl,
	const void		*mwnd)
{
	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

	Class about_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"KPSAboutController"];
	
	if(about_controller_class)
	{
		KPSAboutController *about_controller = [[about_controller_class alloc] init:plugin_version_string];
		
		if(about_controller)
		{
			NSWindow *the_window = [about_controller getWindow];
			
			if(the_window)
			{
				[NSApp runModalForWindow:the_window];
				
				[the_window close];
			}
			
			//[about_controller release];
		}
	}
}

