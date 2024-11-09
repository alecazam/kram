// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramTimer.h"

#include "TaskSystem.h"

#if KRAM_WIN
#include <windows.h>
#elif KRAM_APPLE
#include <mach/mach_time.h>
#elif KRAM_ANDROID
#include <trace.h>
#endif

#define nl '\n'

namespace kram {

using namespace STL_NAMESPACE;

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

#elif KRAM_APPLE

static double queryPeriod()
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);

    // https://eclecticlight.co/2020/11/27/inside-m1-macs-time-and-logs/
    // On macOS Intel, nanosecondsPerTick are 1ns (1/1) = 1Ghz.
    // On macOS M1, nanosecondsPerTick are 41.67ns (num/denom = 125/3) = 24Mhz
    // On M2, A16/A17 Pro, and armv8.6-A should be (1/1) = 1Ghz.
    double period = (double)timebase.numer / timebase.denom;
    period *= 1e-9; // convert to seconds

    return period;
}

static uint64_t queryCounter()
{
    // increment when app sleeps
    // return mach_continuous_time();

    // no increment when app sleeps
    return mach_absolute_time();
}

#endif

static const double gQueryPeriod = queryPeriod();
static const uint64_t gStartTime = queryCounter();

double currentTimestamp()
{
    uint64_t delta = queryCounter() - gStartTime;
    return (double)delta * gQueryPeriod;
}

//-------------------

// TODO: also look into the Perfetto binary format and library/api.
// This needs some daemon to flush data to.  Unclear if can route
//  existing api and timings over to calls?
// https://perfetto.dev/docs/instrumentation/tracing-sdk

// TODO: escape strings, but it's just more work
Perf* Perf::_instance = new Perf();

thread_local uint32_t gPerfStackDepth = 0;

PerfScope::PerfScope(const char* name_)
    : name(name_), time(currentTimestamp())
{
    gPerfStackDepth++;

#if KRAM_ANDROID
    // TODO: also ATrace_isEnabled()
    ATrace_beginSection(name, value);
#endif
}

void PerfScope::close()
{
    if (time != 0.0) {
        --gPerfStackDepth;

#if KRAM_ANDROID
        ATrace_endSection();
#endif
        Perf::instance()->addTimer(name, time, currentTimestamp() - time);
        time = 0.0;
    }
}

void addPerfCounter(const char* name, int64_t value)
{
#if KRAM_ANDROID
    // only int64_t support
    ATrace_setCounter(name, value);
#endif

    Perf::instance()->addCounter(name, currentTimestamp(), value);
}

//---------------

Perf::Perf()
{
    // TODO: should set alongsize exe by default
#if KRAM_WIN
    setPerfDirectory("C:/traces/");
#else
    // sandboxed apps won't reach this, but unsandboxed exe can
    setPerfDirectory("/Users/Alec/traces/");
#endif
}

void Perf::setPerfDirectory(const char* directoryName)
{
    _perfDirectory = directoryName;
}

static bool useTempFile = false;

bool Perf::start(const char* name, bool isCompressed, uint32_t maxStackDepth)
{
    mylock lock(_mutex);

    if (isRunning()) {
        KLOGW("Perf", "start already called");
        return true;
    }

    const char* ext = isCompressed ? ".perftrace.gz" : ".perftrace";
    sprintf(_filename, "%s%s%s", _perfDirectory.c_str(), name, ext);

    _maxStackDepth = maxStackDepth;

    // write json as binary, so win doesn't replace \n with \r\n
    if (useTempFile) {
        if (!_fileHelper.openTemporaryFile("perf-", ext, "w+b")) {
            KLOGW("Perf", "Could not open perf temp file");
            return false;
        }
    }
    else {
        if (!_fileHelper.open(_filename.c_str(), "w+b")) {
            KLOGW("Perf", "Could not open perf file %s", _filename.c_str());
            return false;
        }
    }

    if (!_stream.open(&_fileHelper, !isCompressed)) {
        _fileHelper.close();
        return false;
    }

    // Perf is considered running after this, since _startTime is non-zero

    // TODO: store _startTime in json starting params
    _startTime = currentTimestamp();

    _threadIdToTidMap.clear();
    _threadNames.clear();

    string buf;

    // displayTimeUnit must be ns (nanos) or ms (micros), default is ms
    // "displayTimeUnit": "ns"
    // want ms since it's less data if nanos truncated
    sprintf(buf, R"({"traceEvents":[%c)", nl);
    write(buf);

    // can store file info here, only using one pid
    uint32_t processId = 0;
    const char* processName = "kram"; // TODO: add platform + config + app?

    sprintf(buf, R"({"name":"process_name","ph":"M","pid":%u,"args":{"name":"%s"}},%c)",
            processId, processName, nl);
    write(buf);

    return true;
}

void Perf::stop()
{
    mylock lock(_mutex);

    if (!isRunning()) {
        KLOGW("Perf", "stop called, but never started");
        return;
    }

    // write end of array and object, and force flush
    bool forceFlush = true;
    string buf;
    sprintf(buf, R"(]}%c)", nl);
    write(buf, forceFlush);

    _stream.close();

    if (useTempFile) {
        bool success = _fileHelper.copyTemporaryFileTo(_filename.c_str());
        if (!success) {
            KLOGW("Perf", "Couldn't move temp file");
        }
    }

    _fileHelper.close();

    _startTime = 0.0;
}

void Perf::openPerftrace()
{
    // system call isn't available on iOS
    // also macOS sandbox prevents open call (could write and then open script).
#if KRAM_MAC
    mylock lock(_mutex);

    // DONE: now open the file in kram-profile by opening it
    // okay to use system, but it uses a global mutex on macOS
    // Unclear if macOS can send compressed perftrace.gz file without failing
    // but uncompressed perftrace file might be openable.
    // Also sandbox and hardened runtime may interfere.

    string buf;
    sprintf(buf, "open %s", _filename.c_str());
    system(buf.c_str());
#endif
}

void Perf::write(const string& str, bool forceFlush)
{
    mylock lock(_mutex);

    _buffer += str;

    if (forceFlush || _buffer.size() >= _stream.compressLimit()) {
        _stream.compress(Slice((uint8_t*)_buffer.data(), _buffer.size()), forceFlush);
        _buffer.clear();
    }
}

uint32_t Perf::addThreadIfNeeded()
{
    auto threadId = getCurrentThread();

    // don't need this, it's already locked by caller
    //mylock lock(_mutex);

    auto it = _threadIdToTidMap.find(threadId);
    if (it != _threadIdToTidMap.end()) {
        return it->second;
    }

    // add the new name and tid
    char threadName[kMaxThreadName];
    getThreadName(threadId, threadName);

    // don't really need to store name if not sorting, just need tid counter
    uint32_t tid = _threadNames.size();
    _threadNames.push_back(threadName);

    _threadIdToTidMap.insert(make_pair(threadId, tid));

    // this assumes the map is wiped each time
    string buf;
    sprintf(buf, R"({"name":"thread_name","ph":"M","tid":%u,"args":{"name":"%s"}},%c)",
            tid, threadName, nl);
    write(buf);

    return tid;
}

void Perf::addTimer(const char* name, double time, double elapsed)
{
    if (!isRunning()) {
        return;
    }

    // About Perfetto ts sorting.  This is now fixed to sort duration.
    // https://github.com/google/perfetto/issues/878

    if (_maxStackDepth && gPerfStackDepth >= _maxStackDepth)
        return;

    // zero out the time, so times are smaller to store
    time -= _startTime;

    // problem with duration is that existing events can overlap the start time
    bool isClamped = time < 0.0;
    if (isClamped) {
        elapsed += time;
        time = 0.0;
    }
    if (elapsed <= 0.0)
        return;

    // Catapult timings are suppoed to be in micros.
    // Convert seconds to micros (as integer), lose nanos.  Note that
    // Perfetto will convert all values to nanos anyways and lacks a ms format.
    // Raw means nanos, and Seconds is too small of a fraction.
    // Also printf does IEEE round to nearest even.
    uint32_t timeDigits = 0; // or 3 for nanos
    time *= 1e6;
    elapsed *= 1e6;

    // TODO: worth aliasing the strings, just replacing one string with another
    // but less chars for id.

    // now lock across isRunning, addThread, and write call
    mylock lock(_mutex);
    if (!isRunning()) {
        return;
    }
    // This requires a lock, so buffering the events would help
    // what about sorting the names instead of first-come, first-serve?
    uint32_t tid = addThreadIfNeeded();

    // write out the event in micros, default is displayed in ms
    string buf;
    sprintf(buf, R"({"name":"%s","ph":"X","tid":%d,"ts":%.*f,"dur":%.*f},%c)",
            name, tid, timeDigits, time, timeDigits, elapsed, nl);
    write(buf);
}

// Can also use begin/end but these aren't a atomic
//  R"({"name":"%s","ph":"B","tid":%d,"ts":%.0f},%c)",
//  R"({"ph":"E","tid":%d,"ts":%.0f},%c)",

void Perf::addCounter(const char* name, double time, int64_t amount)
{
    if (!isRunning()) {
        return;
    }

    // also reject nested counters off perf stack depth
    if (_maxStackDepth && gPerfStackDepth >= _maxStackDepth)
        return;

    // zero out the time, so times are smaller to store
    time -= _startTime;

    // problem with duration is that events can occur outside the start time
    if (time < 0.0) {
        return;
    }

    // Catapult timings are suppoed to be in micros.
    // Convert seconds to micros (as integer), lose nanos.  Note that
    // Perfetto will convert all values to nanos anyways.
    // Raw means nanos, and Seconds is too small of a fraction.
    // Also printf does IEEE round to nearest even.
    // https://github.com/google/perfetto/issues/879

    time *= 1e6;
    uint32_t timeDigits = 0; // or 3 for nanos

    // TODO: worth aliasing the strings?, just replacing one string with another
    // but less chars for id.

    // Note: can also have multiple named values passed in args
    // Note: unclear if Perfetto can handle negative values

    // write out the event in micros, default is displayed in ms
    // lld not portable to Win
    string buf;
    sprintf(buf, R"({"name":"%s","ph":"C","ts":%.*f,"args":{"v":%lld}},%c)",
            name, timeDigits, time, amount, nl);
    write(buf);
}

} // namespace kram
