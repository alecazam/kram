/*
    Copyright 2015 Adobe Systems Incorporated
    Distributed under the MIT License (see license at http://stlab.adobe.com/licenses.html)
    
    This file is intended as example code and is not production quality.
*/

/**************************************************************************************************/

#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

/**************************************************************************************************/

namespace kram {

using namespace std;

/**************************************************************************************************/

using lock_t = unique_lock<mutex>;

class notification_queue {
    deque<function<void()>> _q;
    bool _done = false;
    mutex _mutex;
    condition_variable _ready;

public:
    bool try_pop(function<void()>& x)
    {
        lock_t lock{_mutex, try_to_lock};
        if (!lock || _q.empty()) {
            return false;
        }
        x = move(_q.front());
        _q.pop_front();
        return true;
    }

    bool pop(function<void()>& x)
    {
        lock_t lock{_mutex};
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
            lock_t lock{_mutex, try_to_lock};
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
            lock_t lock{_mutex};
            _q.emplace_back(forward<F>(f));
        }

        // allow a waiting pop() to awaken
        _ready.notify_one();
    }

    // has queue been marked done or not
    bool is_done() const
    {
        lock_t lock{const_cast<mutex&>(_mutex)};  // ugh
        bool done_ = _done;
        return done_;
    }

    // mark all tasks submitted to queue, and can start to shutdown
    void set_done()
    {
        {
            lock_t lock{_mutex};
            _done = true;
        }
        _ready.notify_all();
    }
};

/**************************************************************************************************/

#define NOT_COPYABLE(type)      \
    type(const type&) = delete; \
    void operator=(const type&) = delete

class task_system {
    NOT_COPYABLE(task_system);

    const int _count;
    vector<thread> _threads;

    // currently one queue to each thread, but can steal from other queues
    vector<notification_queue> _q;
    atomic<int> _index;

    void run(int threadIndex)
    {
        while (true) {
            // pop() wait avoids a spinloop.

            function<void()> f;

            // start with ours, but steal from other queues if nothing found
            // Note that if threadIndex queue is empty and stays empty
            // then pop() below will stop using that thread.  But async_ is round-robining
            // all work across the available queues.
            int multiple = 4;  // 32;
            int numTries = 0;
            for (int n = 0, nEnd = _count * multiple; n < nEnd; ++n) {
                numTries++;

                // break for loop if work found
                if (_q[(threadIndex + n) % _count].try_pop(f)) {
                    break;
                }
            }

            // numTries is 64 when queues are empty, and typically 1 when queues are full
            //KLOGD("task_system", "thread %d searched %d tries", threadIndex, numTries);

            // if no task, and nothing to steal, pop own queue if possible
            // pop blocks until it's queue receives tasks
            if (!f && !_q[threadIndex].pop(f)) {
                // shutdown if tasks have all been submitted and queue marked as done.
                if (_q[threadIndex].is_done()) {
                    KLOGD("task_system", "thread %d shutting down", threadIndex);

                    break;
                }
                else {
                    KLOGD("task_system", "no work found for %d in %d tries", threadIndex, numTries);

                    // keep searching
                    continue;
                }
            }

            // do the work
            f();
        }
    }

public:
    task_system(int count = 1) : _count(std::min(count, (int)thread::hardware_concurrency())), _q{(size_t)_count}, _index(0)
    {
        // start up the threads
        for (int threadIndex = 0; threadIndex != _count; ++threadIndex) {
            _threads.emplace_back([&, threadIndex] { run(threadIndex); });
        }
    }

    ~task_system()
    {
        // indicate that all tasks are submitted
        for (auto& e : _q) e.set_done();

        // wait until threads are all done, but joining each thread
        for (auto& e : _threads) e.join();
    }

    int num_threads() const
    {
        return _count;
    }

    template <typename F>
    void async_(F&& f)
    {
        auto i = _index++;

        // Note: this isn't a balanced distribution of work
        // but work stealing from other queues in the run() call.
        // Doesn't seem like we need to distribute work here.  Work stealing will pull.

        // push to the next queue that is available
        // this was meant to avoid mutex stalls using a try_lock
        //        for (int n = 0; n != _count; ++n)
        //        {
        //            if (_q[(i + n) % _count].try_push(forward<F>(f))) return;
        //        }

        // otherwise just push to the next indexed queue
        _q[i % _count].push(forward<F>(f));
    }
};

/**************************************************************************************************/

// this autogens max threads even if none are used
//task_system _system;

/**************************************************************************************************/

// There's already a std::future, so may want to look at that
//  otherwise, this all implements async call which would be useful
//
//template <typename>
//struct result_of_;
//
//template <typename R, typename... Args>
//struct result_of_<R(Args...)> { using type = R; };
//
//template <typename F>
//using result_of_t_ = typename result_of_<F>::type;
//
///**************************************************************************************************/
//
//template <typename R>
//struct shared_base {
//    vector<R> _r; // optional
//    mutex _mutex;
//    condition_variable _ready;
//    vector<function<void()>> _then;
//
//    virtual ~shared_base() { }
//
//    void set(R&& r) {
//        vector<function<void()>> then;
//        {
//            lock_t lock{_mutex};
//            _r.push_back(move(r));
//            swap(_then, then);
//        }
//        _ready.notify_all();
//        for (const auto& f : then) _system.async_(move(f));
//    }
//
//    template <typename F>
//    void then(F&& f) {
//        bool resolved{false};
//        {
//            lock_t lock{_mutex};
//            if (_r.empty()) _then.push_back(forward<F>(f));
//            else resolved = true;
//        }
//        if (resolved) _system.async_(move(f));
//    }
//
//    const R& get() {
//        lock_t lock{_mutex};
//        while (_r.empty()) _ready.wait(lock);
//        return _r.back();
//    }
//};
//
//template <typename> struct shared; // not defined
//
//template <typename R, typename... Args>
//struct shared<R(Args...)> : shared_base<R> {
//    function<R(Args...)> _f;
//
//    template<typename F>
//    shared(F&& f) : _f(forward<F>(f)) { }
//
//    template <typename... A>
//    void operator()(A&&... args) {
//        this->set(_f(forward<A>(args)...));
//        _f = nullptr;
//    }
//};
//
//template <typename> class packaged_task; //not defined
//template <typename> class future;
//
//template <typename S, typename F>
//auto package(F&& f) -> pair<packaged_task<S>, future<result_of_t_<S>>>;
//
//template <typename R>
//class future {
//    shared_ptr<shared_base<R>> _p;
//
//    template <typename S, typename F>
//    friend auto package(F&& f) -> pair<packaged_task<S>, future<result_of_t_<S>>>;
//
//    explicit future(shared_ptr<shared_base<R>> p) : _p(move(p)) { }
// public:
//    future() = default;
//
//    template <typename F>
//    auto then(F&& f) {
//        auto pack = package<result_of_t<F(R)>()>([p = _p, f = forward<F>(f)](){
//            return f(p->_r.back());
//        });
//        _p->then(move(pack.first));
//        return pack.second;
//    }
//
//    const R& get() const { return _p->get(); }
//};
//
//template<typename R, typename ...Args >
//class packaged_task<R (Args...)> {
//    weak_ptr<shared<R(Args...)>> _p;
//
//    template <typename S, typename F>
//    friend auto package(F&& f) -> pair<packaged_task<S>, future<result_of_t_<S>>>;
//
//    explicit packaged_task(weak_ptr<shared<R(Args...)>> p) : _p(move(p)) { }
//
// public:
//    packaged_task() = default;
//
//    template <typename... A>
//    void operator()(A&&... args) const {
//        auto p = _p.lock();
//        if (p) (*p)(forward<A>(args)...);
//    }
//};
//
//template <typename S, typename F>
//auto package(F&& f) -> pair<packaged_task<S>, future<result_of_t_<S>>> {
//    auto p = make_shared<shared<S>>(forward<F>(f));
//    return make_pair(packaged_task<S>(p), future<result_of_t_<S>>(p));
//}
//
///**************************************************************************************************/
//
//template <typename F, typename ...Args>
//auto async(F&& f, Args&&... args)
//{
//    using result_type = result_of_t<F (Args...)>;
//    using packaged_type = packaged_task<result_type()>;
//
//    auto pack = package<result_type()>(bind(forward<F>(f), forward<Args>(args)...));
//
//    _system.async_(move(get<0>(pack)));
//    return get<1>(pack);
//}

/**************************************************************************************************/

//int main() {
//    future<cpp_int> x = async([]{ return fibonacci<cpp_int>(100); });
//
//    future<cpp_int> y = x.then([](const cpp_int& x){ return cpp_int(x * 2); });
//    future<cpp_int> z = x.then([](const cpp_int& x){ return cpp_int(x / 15); });
//
//    cout << y.get() << endl;
//    cout << z.get() << endl;
//}

}  // namespace kram
