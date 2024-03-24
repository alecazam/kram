// Clang Build Analyzer https://github.com/aras-p/ClangBuildAnalyzer
// SPDX-License-Identifier: Unlicense

#import "CBA.h"

// This is for windows.h
//#define _CRT_SECURE_NO_WARNINGS
//#define NOMINMAX

#include "Analysis.h"
#include "Arena.h"
#include "BuildEvents.h"
//#include "Colors.h"
#include "Utils.h"

#include <stdio.h>
#include <string>
#include <time.h>
#include <algorithm>
#include <set>
#include <cassert>

//#ifdef _MSC_VER
//struct IUnknown; // workaround for old Win SDK header failures when using /permissive-
//#endif

    
@implementation CBA {
}

+ (NSString* _Nonnull)RunCBA:(NSArray<NSData*> * _Nonnull)files filenames:(NSArray<NSString*> * _Nonnull)filenames
{
    ArenaInitialize();
    
    BuildEventsParser* parser = CreateBuildEventsParser();
   
    for (uint32_t i = 0; i < files.count; ++i)
    {
        NSData* data = files[i];
        ParseBuildEvents(parser, (const uint8_t*)data.bytes, data.length, [filenames[i] UTF8String]);
    }

    // Run the analysis on data from the parser.
    std::string out;
    DoAnalysis(GetBuildEvents(*parser), GetBuildNames(*parser), out);
    
    // Shutdown the parser
    DeleteBuildEventsParser(parser);
    ArenaDelete();
    
    return [NSString stringWithUTF8String:out.c_str()];
}

@end

