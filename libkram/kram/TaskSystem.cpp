#include "TaskSystem.h"

// TODO: bury in system/cpp file
#if KRAM_MAC || KRAM_IOS
    #include <mach/thread_act.h>
    #include <mach/thread_policy.h>
#elif KRAM_WIN
    #include <windows.h>
#else
    #include <pthread/pthread.h>
#endif

namespace kram {
using namespace NAMESPACE_STL;

void task_system::set_affinity(std::thread& thread, uint32_t threadIndex)
{
    // https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
    // TODO: set affinity, but need to create a thread that doesn't launch
    // so can set this up, and then run it.
    
    auto handle = thread.native_handle();
    uint64_t affinityMask = ((uint64_t)1) << threadIndex; // for now only allow single thread mask
            
#if KRAM_MAC || KRAM_IOS
    thread_affinity_policy_data_t policy = { (int)affinityMask };

    // TODO: check return
    thread_policy_set(pthread_mach_thread_np(handle), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);

#elif KRAM_WIN
    // each processor group only has 64 bits
    SetThreadAffinityMask(handle, &affinityMask);
#else
    // most systems are pthread-based, this is represented with array of bits
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIndex, &cpuset);

    // TODO: check return
    /*int rc = */ pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#endif
}

void task_system::run(int32_t threadIndex)
{
    while (true) {
        // pop() wait avoids a spinloop.

        function<void()> f;

        // start with ours, but steal from other queues if nothing found
        // Note that if threadIndex queue is empty and stays empty
        // then pop() below will stop using that thread.  But async_ is round-robining
        // all work across the available queues.
        int32_t multiple = 4;  // 32;
        int32_t numTries = 0;
        for (int32_t n = 0, nEnd = _count * multiple; n < nEnd; ++n) {
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


// TODO: don't want hyperthreads from hardware_concurrency
task_system::task_system(int32_t count) :
    _count(std::min(count, (int32_t)std::thread::hardware_concurrency())),
    _q{(size_t)_count},
    _index(0)
{
    // start up the threads
    for (int32_t threadIndex = 0; threadIndex != _count; ++threadIndex) {
        _threads.emplace_back([&, threadIndex] { run(threadIndex); });
        set_affinity(_threads.back(), threadIndex);
    }
    
}

task_system::~task_system()
{
    // indicate that all tasks are submitted
    for (auto& e : _q)
        e.set_done();

    // wait until threads are all done, but joining each thread
    for (auto& e : _threads)
        e.join();
}

}


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

//int32_t main() {
//    future<cpp_int> x = async([]{ return fibonacci<cpp_int>(100); });
//
//    future<cpp_int> y = x.then([](const cpp_int& x){ return cpp_int(x * 2); });
//    future<cpp_int> z = x.then([](const cpp_int& x){ return cpp_int(x / 15); });
//
//    cout << y.get() << endl;
//    cout << z.get() << endl;
//}
