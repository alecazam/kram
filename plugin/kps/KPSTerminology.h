
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

#ifndef DDSTerminology_H
#define DDSTerminology_H

//-------------------------------------------------------------------------------
//	Options
//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
//	Definitions -- Scripting keys
//-------------------------------------------------------------------------------

#define keyDDSformat			'kkfm'
//#define keyDDSalpha			'DDSa'
//#define keyDDSpremult			'DDSp'
//#define keyDDSmipmap			'DDSm'
//#define keyDDSfilter			'DDSq'
//#define keyDDScubemap			'DDSc'

#define typeDDSformat			'ktfm'

#define formatBC1				'BC1 '
#define formatBC3				'BC3 '
#define formatBC4				'BC4 '
#define formatBC5				'BC5 '
#define formatBC7               'BC7 '

// signed and srgb variants
#define formatBC1S              'BC1S'
#define formatBC3S              'BC3S'
#define formatBC4S              'BC4S'
#define formatBC5S              'BC5S'
#define formatBC7S              'BC7S'

// explicit
#define formatR8                'U8r '
#define formatRG8               'U8rg'
#define formatRGBA8		        'U84 '
#define formatRGBA8S            'U84S'

#define formatR16F              'H4r '
#define formatRG16F             'H4rg'
#define formatRGBA16F           'H4  '

#define formatR32F              'F4r '
#define formatRG32F             'F4rg'
#define formatRGBA32F           'F4  '

// TODO: signed RGBA8?
// TODO: ASTC
// TODO: ETC

//#define typeAlphaChannel		'alfT'
//
//#define alphaChannelNone		'Nalf'
//#define alphaChannelTransparency 'Talf'
//#define alphaChannelChannel		'Calf'
//
//#define typeFilter				'filT'
//
//#define filterBox				'Bfil'
//#define filterTent				'Tfil'
//#define filterLanczos4			'Lfil'
//#define filterMitchell			'Mfil'
//#define filterKaiser			'Kfil'

#endif
