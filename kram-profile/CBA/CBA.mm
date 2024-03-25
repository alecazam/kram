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
    BuildEventsParser* parser;
}

- (_Nonnull instancetype)init {
    ArenaInitialize();
    
    parser = CreateBuildEventsParser();
    
    return self;
}

- (void)deinit {
    // Shutdown the parser
    DeleteBuildEventsParser(parser);
    parser = nullptr;
    
    ArenaDelete();
}

// This is bad because it runs single-threaded, and doesn't cache anything across builds.
// TODO: restructure, so parser is built once
// feed files to it individually, and then request analysis on a few of the events/names
// TODO: reformat output to Perfetto json, can then display it visually.
- (void)parseAll:(NSArray<NSData*> * _Nonnull)files filenames:(NSArray<NSString*> * _Nonnull)filenames
{
    for (uint32_t i = 0; i < files.count; ++i) {
        [self parse:files[i] filename:filenames[i]];
    }
}

- (void)parse:(NSData* _Nonnull)file filename:(NSString* _Nonnull)filename {
    const char* filename_ = [filename UTF8String];
    ParseBuildEvents(parser, (const uint8_t*)file.bytes, file.length, filename_);
}


- (NSString* _Nonnull)analyze {
    // Run the analysis on data from the parser.
    std::string out;
    DoAnalysis(GetBuildEvents(*parser), GetBuildNames(*parser), out);
    
    return [NSString stringWithUTF8String:out.c_str()];
}



@end

