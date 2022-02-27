// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramTimer.h"

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

double currentTimestamp()
{
    using clock = high_resolution_clock;
    //using clock = system_clock;
    //using clock = steady_clock;

    static clock::time_point startTime = clock::now();

    auto duration = duration_cast<nanoseconds>(clock::now() - startTime);
    return (double)duration.count() * 1e-9;
}

}  // namespace kram
