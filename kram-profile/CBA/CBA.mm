// Clang Build Analyzer https://github.com/aras-p/ClangBuildAnalyzer
// SPDX-License-Identifier: Unlicense

#import "CBA.h"

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

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

#ifdef _MSC_VER
struct IUnknown; // workaround for old Win SDK header failures when using /permissive-
#endif

    
@implementation CBA {
}

// Logs out to stdout right now.  But will change.
+ (void)RunCBA:(NSArray<NSData*> *)files filenames:(NSArray<NSString*> *)filenames
{
    ArenaInitialize();
    
    BuildEventsParser* parser = CreateBuildEventsParser();
   
    std::vector<uint8_t> buffer;
    
    for (uint32_t i = 0; i < files.count; ++i)
    {
        // extract the file from disk or archive,
        //   run it through simdjson
        // and then feed the results to the parser
        //if (!zip.extract(entry.filename, buffer))
        //    continue;
        NSData* data = files[i];
        
        ParseBuildEvents(parser, (const uint8_t*)data.bytes, data.length, [filenames[i] UTF8String]);
    }

    // Run the analysis on data from the parser.
    DoAnalysis(GetBuildEvents(*parser), GetBuildNames(*parser), stdout);
    
    // Shutdown the parser
    DeleteBuildEventsParser(parser);
    ArenaDelete();
}

@end

