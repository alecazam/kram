
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

#if _WIN32

#include "DDS.h"

#include "DDS_UI.h"
#include "DDS_version.h"

#include <Windows.h>

#include <assert.h>

enum {
	OUT_noUI = -1,
	OUT_OK = IDOK,
	OUT_Cancel = IDCANCEL,
	OUT_Format_Menu,
	OUT_Mipmap_Check,
	OUT_Filter_Menu,
	OUT_Filter_Menu_Label,
	OUT_Alpha_Radio_None,
	OUT_Alpha_Radio_Transparency,
	OUT_Alpha_Radio_Channel,
	OUT_Premultiply_Check,
	OUT_Alpha_Frame,
	OUT_CubeMap_Check
};

// sensible Win macros
#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

#define ENABLE_ITEM(ITEM, ENABLE)	EnableWindow(GetDlgItem(hwndDlg, (ITEM)), (ENABLE));

#define SHOW_ITEM(ITEM, SHOW)	ShowWindow(GetDlgItem(hwndDlg, (ITEM)), (SHOW) ? SW_SHOW : SW_HIDE)



static DialogFormat			g_format = DIALOG_FMT_DXT5;
static DialogAlpha			g_alpha = DIALOG_ALPHA_NONE;
static bool					g_premultiply = false;
static bool					g_mipmap = false;
static Dialog_Filter		g_filter = DIALOG_FILTER_MITCHELL;
static bool					g_cubemap = false;

static bool					g_have_transparency = false;
static const char			*g_alpha_name = NULL;
static bool					g_ae_ui = false;

static WORD	g_item_clicked = 0;


static void TrackMipmap(HWND hwndDlg)
{
	BOOL enable_state = GET_CHECK(OUT_Mipmap_Check);
	ENABLE_ITEM(OUT_Filter_Menu, enable_state);
	ENABLE_ITEM(OUT_Filter_Menu_Label, enable_state);
}


static void TrackAlpha(HWND hwndDlg)
{
	BOOL enable_state = !GET_CHECK(OUT_Alpha_Radio_None);
	ENABLE_ITEM(OUT_Premultiply_Check, enable_state);
}


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
			do{
				// set up the menu
				// I prefer to do it programatically to insure that the compression types match the index
				const char *opts[] = {	"DXT1",
										"DXT1A",
										"DXT2",
										"DXT3",
										"DXT4",
										"DXT5",
										"DXT5A",
										"3Dc",
										"DXN",
										"Uncompressed" };

				HWND menu = GetDlgItem(hwndDlg, OUT_Format_Menu);

				for(int i=DIALOG_FMT_DXT1; i <= DIALOG_FMT_UNCOMPRESSED; i++)
				{
					SendMessage(menu, (UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)opts[i] );
					SendMessage(menu, (UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the compresion number

					if(i == g_format)
						SendMessage(menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
				}


				const char *f_opts[] = {"Box",
										"Tent",
										"Lanczos4",
										"Mitchell",
										"Kaiser" };

				HWND f_menu = GetDlgItem(hwndDlg, OUT_Filter_Menu);

				for(int i=DIALOG_FILTER_BOX; i <= DIALOG_FILTER_KAISER; i++)
				{
					SendMessage(f_menu, (UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)f_opts[i] );
					SendMessage(f_menu, (UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the compresion number

					if(i == g_filter)
						SendMessage(f_menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
				}
			}while(0);

			SET_CHECK(OUT_Mipmap_Check, g_mipmap);

			if(!g_have_transparency)
			{
				ENABLE_ITEM(OUT_Alpha_Radio_Transparency, FALSE);

				if(g_alpha == DIALOG_ALPHA_TRANSPARENCY)
				{
					g_alpha = (g_alpha_name != NULL ? DIALOG_ALPHA_CHANNEL : DIALOG_ALPHA_NONE);
				}
			}

			if(g_alpha_name == NULL)
			{
				ENABLE_ITEM(OUT_Alpha_Radio_Channel, FALSE);

				if(g_alpha == DIALOG_ALPHA_CHANNEL)
				{
					g_alpha = (g_have_transparency ? DIALOG_ALPHA_TRANSPARENCY : DIALOG_ALPHA_NONE);
				}
			}
			else
			{
				SetDlgItemText(hwndDlg, OUT_Alpha_Radio_Channel, g_alpha_name);
			}

			SET_CHECK(OUT_Premultiply_Check, g_premultiply);

			SET_CHECK( (g_alpha == DIALOG_ALPHA_NONE ? OUT_Alpha_Radio_None :
						g_alpha == DIALOG_ALPHA_TRANSPARENCY ? OUT_Alpha_Radio_Transparency :
						g_alpha == DIALOG_ALPHA_CHANNEL ? OUT_Alpha_Radio_Channel :
						OUT_Alpha_Radio_None), TRUE);
			
			SET_CHECK(OUT_CubeMap_Check, g_cubemap);

			TrackAlpha(hwndDlg);
			TrackMipmap(hwndDlg);

			if(g_ae_ui)
			{
				for(int i = OUT_Alpha_Radio_None; i <= OUT_Alpha_Frame; i++)
					SHOW_ITEM(i, false);

				WINDOWPLACEMENT winPlace, cubemapPlace, okPlace, cancelPlace;
				winPlace.length = cubemapPlace.length = okPlace.length = cancelPlace.length = sizeof(WINDOWPLACEMENT);

				GetWindowPlacement(hwndDlg, &winPlace);
				GetWindowPlacement(GET_ITEM(OUT_CubeMap_Check), &cubemapPlace);
				GetWindowPlacement(GET_ITEM(OUT_OK), &okPlace);
				GetWindowPlacement(GET_ITEM(OUT_Cancel), &cancelPlace);

				const int resize = 170;

				winPlace.rcNormalPosition.bottom -= resize;
				cubemapPlace.rcNormalPosition.top -= resize;
				cubemapPlace.rcNormalPosition.bottom -= resize;
				okPlace.rcNormalPosition.top -= resize;
				okPlace.rcNormalPosition.bottom -= resize;
				cancelPlace.rcNormalPosition.top -= resize;
				cancelPlace.rcNormalPosition.bottom -= resize;

				SetWindowPlacement(GET_ITEM(OUT_CubeMap_Check), &cubemapPlace);
				SetWindowPlacement(GET_ITEM(OUT_Cancel), &cancelPlace);
				SetWindowPlacement(GET_ITEM(OUT_OK), &okPlace);
				SetWindowPlacement(hwndDlg, &winPlace);
			}

			return TRUE;
 
		case WM_NOTIFY:
			return FALSE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch(g_item_clicked)
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different
				do{
					HWND menu = GetDlgItem(hwndDlg, OUT_Format_Menu);

					// get the channel index associated with the selected menu item
					LRESULT cur_sel = SendMessage(menu,(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);

					g_format = (DialogFormat)SendMessage(menu, (UINT)CB_GETITEMDATA, (WPARAM)cur_sel, (LPARAM)0);

					g_alpha =	GET_CHECK(OUT_Alpha_Radio_None) ? DIALOG_ALPHA_NONE :
								GET_CHECK(OUT_Alpha_Radio_Transparency) ? DIALOG_ALPHA_TRANSPARENCY :
								GET_CHECK(OUT_Alpha_Radio_Channel) ? DIALOG_ALPHA_CHANNEL :
								DIALOG_ALPHA_TRANSPARENCY;

					g_premultiply = GET_CHECK(OUT_Premultiply_Check);

					g_mipmap = GET_CHECK(OUT_Mipmap_Check);

					HWND f_menu = GetDlgItem(hwndDlg, OUT_Filter_Menu);
					cur_sel = SendMessage(f_menu,(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					g_filter = (Dialog_Filter)SendMessage(f_menu, (UINT)CB_GETITEMDATA, (WPARAM)cur_sel, (LPARAM)0);

					g_cubemap = GET_CHECK(OUT_CubeMap_Check);

					EndDialog(hwndDlg, 0);
					return TRUE;
				}while(0);

				case OUT_Alpha_Radio_None:
				case OUT_Alpha_Radio_Transparency:
				case OUT_Alpha_Radio_Channel:
					TrackAlpha(hwndDlg);
					return TRUE;


				case OUT_Mipmap_Check:
					TrackMipmap(hwndDlg);
					return TRUE;
            } 
    } 
    return FALSE; 
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
	g_format		= params->format;
	g_alpha			= params->alpha;
	g_premultiply	= params->premultiply;
	g_mipmap		= params->mipmap;
	g_filter		= params->filter;
	g_mipmap		= params->mipmap;
	
	g_have_transparency = have_transparency;
	g_alpha_name = alpha_name;
	g_ae_ui = ae_ui;

	if(ae_ui)
	{
		g_alpha = DIALOG_ALPHA_TRANSPARENCY;
		g_premultiply = false;
		assert(g_alpha_name == NULL);
	}

	int status = DialogBox((HINSTANCE)plugHndl, (LPSTR)"OUT_DIALOG", (HWND)mwnd, (DLGPROC)DialogProc);


	if(g_item_clicked == OUT_OK)
	{
		params->format			= g_format;
		params->alpha			= g_alpha;
		params->premultiply		= g_premultiply;
		params->mipmap			= g_mipmap;
		params->filter			= g_filter;
		params->cubemap			= g_cubemap;

		return true;
	}
	else
		return false;
}


enum {
	ABOUT_noUI = -1,
	ABOUT_OK = IDOK,
	ABOUT_Plugin_Version_String = 4,
};

static const char *g_plugin_version_string = NULL;

static BOOL CALLBACK AboutProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
				SetDlgItemText(hwndDlg, ABOUT_Plugin_Version_String, g_plugin_version_string);

			return TRUE;
 
		case WM_NOTIFY:
			return FALSE;

        case WM_COMMAND: 
            switch(LOWORD(wParam))
            { 
                case OUT_OK: 
				case OUT_Cancel:
					EndDialog(hwndDlg, 0);
					return TRUE;
            } 
    } 
    return FALSE; 
} 

void
DDS_About(
	const char		*plugin_version_string,
	const void		*plugHndl,
	const void		*mwnd)
{
	g_plugin_version_string = plugin_version_string;

	int status = DialogBox((HINSTANCE)plugHndl, (LPSTR)"ABOUT_DIALOG", (HWND)mwnd, (DLGPROC)AboutProc);
}

#endif
