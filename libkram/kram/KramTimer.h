// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <cassert>

// These are only here for Perf class
#include "KramFileHelper.h"
#include "KramZipStream.h"

//#include "KramConfig.h"

namespace kram {
// Can obtain a timestamp to nanosecond accuracy.
double currentTimestamp();

// This can record timings for each start/stop call.
class Timer {
public:
    Timer(bool createStarted = true)
    {
        if (createStarted) {
            start();
        }
    }
    void start()
    {
        assert(_timeElapsed >= 0.0);
        _timeElapsed -= currentTimestamp();
    }
    
    void stop()
    {
        assert(_timeElapsed < 0.0);
        _timeElapsed += currentTimestamp();
    }
    
    double timeElapsed() const
    {
        double time = _timeElapsed;
        if (time < 0.0) {
            time += currentTimestamp();
        }
        return time;
    }
    
    double timeElapsedMillis() const
    {
        return timeElapsed() * 1e3;
    }
    
    bool isStopped() const { return _timeElapsed < 0.0; }
    
private:
    double _timeElapsed = 0.0;
};

// This scope records add to  timeElapsed if the timer is stopped alredy.
class TimerScope {
public:
    TimerScope(Timer& timer)
    {
        if (timer.isStopped()) {
            _timer = &timer;
            _timer->start();
        }
    }
    
    ~TimerScope()
    {
        close();
    }
    
    void close()
    {
        if (_timer) {
            _timer->stop();
            _timer = nullptr;
        }
    }
    
private:
    Timer* _timer = nullptr;
};
    
// This implements PERF macros, sending timing data to kram-profile, perfetto, and/or Tracy.
class Perf {
public:
    bool isRunning() const { return _startTime != 0.0; }
    
    bool start(const char* filename, uint32_t maxStackDepth = 0);
    void stop();
    
    void addTimer(const char* name, double time, double elapsed);
    void addCounter(const char* name, double time, int64_t value);
    
    // singleton getter, but really want to split Perf from macros.
    static Perf* instance() { return _instance; }
    
    // on it's own track/tid, add a frame vsync marker
    // TODO: void addFrameMarker(double time);
    
private:
    void write(const string& str, bool forceFlush = false);
    uint32_t addThreadIfNeeded();
    
    ZipStream  _stream;
    FileHelper _fileHelper;
    double _startTime = 0.0;
    string _filename;
    
    using mymutex = recursive_mutex;
    using mylock = unique_lock<mymutex>;

    mymutex _mutex;
    unordered_map<thread::native_handle_type, uint32_t> _threadIdToTidMap;
    vector<string> _threadNames;
    string _buffer;
    uint32_t _maxStackDepth = 0; // 0 means no limit
    
    static Perf* _instance;
};

class PerfScope {
public:
    // This means that the timers are running even when not profiling
    PerfScope(const char* name_);
    ~PerfScope() { close(); }
    
    void close();
    
private:
    const char* name;
    double time;
};

// This is here to split off Perf
void addPerfCounter(const char* name, int64_t value);

#define KPERF_SCOPENAME2(a,b) scope ## b
#define KPERF_SCOPENAME(b) KPERF_SCOPENAME2(scope,b)

#define KPERFT(x) PerfScope KPERF_SCOPENAME(__COUNTER__)(x)

#define KPERFT_START(num,x) PerfScope KPERF_SCOPENAME(num)(x)
#define KPERFT_STOP(num) KPERF_SCOPENAME(num).close()

#define KPERFC(x, value) addPerfCounter(x, value)

}  // namespace kram


   
