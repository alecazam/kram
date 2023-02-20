// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <cassert>

//#include "KramConfig.h"

namespace kram {
// Can obtain a timestamp to nanosecond accuracy.
extern double currentTimestamp();

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

}  // namespace kram
