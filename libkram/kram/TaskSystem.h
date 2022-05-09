/*
    Copyright 2015 Adobe Systems Incorporated
    Distributed under the MIT License (see license at http://stlab.adobe.com/licenses.html)
    
    This file is intended as example code and is not production quality.
*/

/**************************************************************************************************/

//#include <algorithm>
//#include <atomic>
//#include <deque>
//#include <functional>
//#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
//#include <vector>



/**************************************************************************************************/

namespace kram {
using namespace NAMESPACE_STL;

/**************************************************************************************************/

using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;
using mycondition = std::condition_variable_any;

class notification_queue {
    deque<function<void()>> _q;
    bool _done = false;
    mymutex _mutex;
    mycondition _ready;

public:
    bool try_pop(function<void()>& x)
    {
        mylock lock{_mutex, std::try_to_lock};
        if (!lock || _q.empty()) {
            return false;
        }
        x = move(_q.front());
        _q.pop_front();
        return true;
    }

    bool pop(function<void()>& x)
    {
        mylock lock{_mutex};
        while (_q.empty() && !_done) {
            _ready.wait(lock);  // this is what blocks a given thread to avoid spin loop
        }

        // handle done state
        if (_q.empty()) {
            return false;
        }

        // return the work while lock is held
        x = move(_q.front());
        _q.pop_front();
        return true;
    }

    template <typename F>
    bool try_push(F&& f)
    {
        {
            mylock lock{_mutex, std::try_to_lock};
            if (!lock) {
                return false;
            }
            _q.emplace_back(forward<F>(f));
        }
        _ready.notify_one();
        return true;
    }

    template <typename F>
    void push(F&& f)
    {
        {
            mylock lock{_mutex};
            // TODO: fix this construct, it's saying no matching sctor for eastl::deque<eastl::function<void ()>>>::value_type
#if USE_EASTL
            KLOGE("TaskSystem", "Fix eastl deque or function");
            //_q.emplace_back(forward<F>(f));
#else
            _q.emplace_back(forward<F>(f));
#endif
        }
        // allow a waiting pop() to awaken
        _ready.notify_one();
    }

    // has queue been marked done or not
    bool is_done() const
    {
        mylock lock{const_cast<mymutex&>(_mutex)};  // ugh
        bool done_ = _done;
        return done_;
    }

    // mark all tasks submitted to queue, and can start to shutdown
    void set_done()
    {
        {
            mylock lock{_mutex};
            _done = true;
        }
        _ready.notify_all();
    }
};

/**************************************************************************************************/

#define NOT_COPYABLE(type)      \
    type(const type&) = delete; \
    void operator=(const type&) = delete

// only for ioS/macOS
enum class ThreadQos
{
    Low = 1,
    Medium = 2,
    Default = 3,
    High = 4,
    Interactive = 5,
};

class task_system {
    NOT_COPYABLE(task_system);

    const int32_t _count;
    vector<std::thread> _threads;

    // currently one queue to each thread, but can steal from other queues
    vector<notification_queue> _q;
    std::atomic<int32_t> _index;

    void run(int32_t threadIndex);

    void set_affinity(std::thread& thread, uint32_t threadIndex);
    void set_qos(std::thread& thread, ThreadQos level);
    
public:
    task_system(int32_t count = 1);
    ~task_system();

    int32_t num_threads() const { return _count; }
    
    template <typename F>
    void async_(F&& f)
    {
        auto i = _index++;

        // Note: this isn't a balanced distribution of work
        // but work stealing from other queues in the run() call.
        // Doesn't seem like we need to distribute work here.  Work stealing will pull.

        // push to the next queue that is available
        // this was meant to avoid mutex stalls using a try_lock
        //        for (int32_t n = 0; n != _count; ++n)
        //        {
        //            if (_q[(i + n) % _count].try_push(forward<F>(f))) return;
        //        }

        // otherwise just push to the next indexed queue
        _q[i % _count].push(forward<F>(f));
    }
};


}  // namespace kram
