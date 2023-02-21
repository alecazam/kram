// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramTimer.h"

#if 1
#if KRAM_WIN
#include <windows.h>
#elif KRAM_MAC || KRAM_IOS
#include <mach/mach_time.h>
#endif

namespace kram {

using namespace NAMESPACE_STL;

#if KRAM_WIN

static double queryPeriod()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    
    // convert from nanos to seconds
    return 1.0 / double(frequency.QuadPart);
};

static uint64_t queryCounter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
};

static const uint64_t gStartTime = queryCounter();
static const double gQueryPeriod = queryPeriod();

double currentTimestamp()
{
    return (double)(queryCounter() - gStartTime) * gQueryPeriod;
}

#elif KRAM_IOS || KRAM_MAC

static uint64_t queryCounter()
{
    // increment when app sleeps
    // return mach_continuous_time();
    
    // no increment when app sleeps
    return mach_absolute_time();
}

static double queryPeriod()
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    
    // https://eclecticlight.co/2020/11/27/inside-m1-macs-time-and-logs/
    // On macOS Intel, nanosecondsPerTick are 1ns (1/1)
    // On macOS M1, nanosecondsPerTick are 41.67ns (num/denom = 125/3)
    double period = (double)timebase.numer / timebase.denom;
    period *= 1e-9; // convert to seconds
    
    return period;
}
static const uint64_t gStartTime = queryCounter();
static const double gQueryPeriod = queryPeriod();

double currentTimestamp()
{
    return (double)(queryCounter() - gStartTime) * gQueryPeriod;
}

}

#endif

#else

/*
// This is the worst timing system.  On macOS, resolution of 32ms even
//   using the high_resolution_clock.
 
// see sources here
// https://codebrowser.dev/llvm/libcxx/src/chrono.cpp.html
// can't find high_resolution_clock source,
// but steady on macOS uses clock_gettime(CLOCK_MONOTONIC_RAW, &tp)
//   which should be mach_continuous_time()
//
// also see sources here for timers
// https://opensource.apple.com/source/Libc/Libc-1158.1.2/gen/clock_gettime.c.auto.html
// mach_continuous_time() vs. mach_absolute_time()
 
#if USE_EASTL
#include "EASTL/chrono.h"
#else
#include <chrono>
#endif
 
namespace kram {

using namespace NAMESPACE_STL;

#if USE_EASTL
using namespace eastl::chrono;
#else
using namespace std::chrono;
#endif

// high-res sucks  (defaults to steady or system in libcxx)
// doesn't matter whether system/stead used, they both have 32ms resolution
//using myclock = high_resolution_clock;
//using myclock = system_clock;
using myclock = steady_clock;

static const myclock::time_point gStartTime = myclock::now();

double currentTimestamp()
{
    auto t = myclock::now();
    duration<double, std::milli> timeSpan = t - gStartTime;
    double count = (double)timeSpan.count() * 1e-3;
    
    // this happens the first time function is called if static
    // is inside the runction call.  Will return 0
    // assert( count != 0.0 );
    
    return count;
}

}  // namespace kram
*/
#endif

