// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramTimer.h"

#if KRAM_WIN
#include <windows.h>
#elif KRAM_MAC || KRAM_IOS
#include <mach/mach_time.h>
#endif

namespace kram {

using namespace NAMESPACE_STL;

#if KRAM_WIN

static double queryFrequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return double(1000000000.0L / frequency.QuadPart);  // nanoseconds per tick
};

static uint64_t queryCounter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
};

static uint64_t gStartTime = queryCounter();
static double gQueryFrequency = queryFrequency();

double currentTimestamp()
{
    return (double)(queryCounter() - gStartTime) * gQueryFrequency;
}

#else

static uint64_t gStartTime = mach_absolute_time();
double currentTimestamp()
{
    return (double)(mach_absolute_time() - gStartTime) * 1e-9;
}

#endif

/* This is the worse timing system ever, with min times of 0.032s even
   using the high_resolution_clock on macOS.
 
#if USE_EASTL
#include "EASTL/chrono.h"
#else
#include <chrono>
#endif
 
#if USE_EASTL
using namespace eastl::chrono;
#else
using namespace std::chrono;
#endif

// high-res sucks
using clock = high_resolution_clock;
//using clock = system_clock;
//using clock = steady_clock;

static clock::time_point startTime = clock::now();

double currentTimestamp()
{
    auto t = clock::now();
    auto timeSpan = duration_cast<duration<double> >(t - startTime);
    double count = (double)timeSpan.count();
    
    // this happens the first time function is called if static
    // is inside the runction call.  Will return 0
    // assert( count != 0.0 );
    
    return count;
}
*/

}  // namespace kram
