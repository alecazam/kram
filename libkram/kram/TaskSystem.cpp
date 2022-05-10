#include "TaskSystem.h"

#if KRAM_MAC
    // affiniity
    #include <mach/thread_act.h>
    #include <mach/thread_policy.h>

    #include <pthread/qos.h>
    #include <pthread/pthread.h>
    #include <sys/sysctl.h>
#elif KRAM_IOS
    #include <pthread/qos.h>
    #include <sys/sysctl.h>
#elif KRAM_WIN
    #include <windows.h>
#else
    #include <pthread/pthread.h>
#endif

namespace kram {
using namespace NAMESPACE_STL;

enum class CoreType : uint8_t
{
    Little,
    // Medium,
    Big,
};

struct CoreNum
{
    uint8_t index;
    CoreType type;
};

struct CoreInfo
{
    // hyperthreading can result in logical = 2x physical cores (1.5x on Alderlake)
    uint32_t logicalCoreCount;
    uint32_t physicalCoreCount;
    
    // ARM is has big-little and big-medium-little, no HT, 2/4, 4/4, 6/2, 8/2.
    // Intel x64 AlderLake has big-little. 24 threads (8x2HT/8)
    uint32_t bigCoreCount;
    uint32_t littleCoreCount;
    
    // x64 under Rosetta2 on M1 Arm chip, no AVX only SSE 4.2
    uint32_t isTranslated;
    uint32_t isHyperthreaded;
    
    vector<CoreNum> remapTable;
};

#if KRAM_WIN
// Helper function to count set bits in the processor mask.
static DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
    DWORD i;
    
    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest /= 2;
    }

    return bitSetCount;
}
#endif

static const CoreInfo& GetCoreInfo()
{
    static CoreInfo coreInfo = {};
    if (coreInfo.logicalCoreCount != 0)
        return coreInfo;

    // this includes hyperthreads
    coreInfo.logicalCoreCount = std::thread::hardware_concurrency();
    coreInfo.physicalCoreCount = coreInfo.logicalCoreCount;
        
    #if KRAM_IOS || KRAM_MAC
    // get big/little core counts
    // use sysctl -a from command line to see all
    size_t size = sizeof(coreInfo.bigCoreCount);
    
    uint32_t perfLevelCount = 0;
    
    // only big-little core counts on macOS12/iOS15
    sysctlbyname("hw.nperflevels", &perfLevelCount, &size, nullptr, 0);
    if (perfLevelCount > 0) {
        sysctlbyname("hw.perflevel0.physicalcpu", &coreInfo.bigCoreCount, &size, nullptr, 0);
        if (perfLevelCount > 1)
            sysctlbyname("hw.perflevel1.physicalcpu", &coreInfo.littleCoreCount, &size, nullptr, 0);
    }
    else {
        // can't identify little cores
        sysctlbyname("hw.perflevel0.physicalcpu", &coreInfo.bigCoreCount, &size, nullptr, 0);
    }
    
    // may not work on A10 2/2 exclusive
    coreInfo.physicalCoreCount = std::min(coreInfo.bigCoreCount + coreInfo.littleCoreCount, coreInfo.physicalCoreCount);
    
    // no affinity, so core order here doesn't really matter.
    for (uint32_t i = 0; i < coreInfo.bigCoreCount; ++i) {
        coreInfo.remapTable.push_back({(uint8_t)i, CoreType::Big});
    }
    for (uint32_t i = 0; i < coreInfo.littleCoreCount; ++i) {
        coreInfo.remapTable.push_back({(uint8_t)(i + coreInfo.bigCoreCount), CoreType::Little});
    }
    
    coreInfo.isHyperthreaded = coreInfo.logicalCoreCount != coreInfo.physicalCoreCount;
    
    #if KRAM_MAC
    // Call the sysctl and if successful return the result
    sysctlbyname("sysctl.proc_translated", &coreInfo.isTranslated, &size, NULL, 0);
    #endif
    
    #elif KRAM_WIN
    
    // have to walk array of data, and assemble this info, ugh
    // https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformation
    
    DWORD logicalCoreCount = 0;
    DWORD physicalCoreCount = 0;
      
    DWORD returnLength = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
    DWORD rc = GetLogicalProcessorInformation(buffer, &returnLength);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = nullptr;
    DWORD byteOffset = 0;
    
    // walk the array
    bool isHyperthreaded = false;
    ptr = buffer;
    byteOffset = 0;
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
        switch (ptr->Relationship) {
            case RelationProcessorCore: {
                uint32_t logicalCores = CountSetBits(ptr->ProcessorMask);
                if (logicalCores > 1) {
                    isHyperthreaded = true;
                }
                break;
            }
        }
        
        if (isHyperthreaded)
            break;
        
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    ptr = buffer;
    byteOffset = 0;
    uint32_t coreNumber = 0;
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
        switch (ptr->Relationship) {
            case RelationProcessorCore: {
                physicalCoreCount++;
                
                // A hyperthreaded core supplies more than one logical processor.
                // Can identify AlderLake big vs. little off this
                uint32_t logicalCores = CountSetBits(ptr->ProcessorMask);
                if (logicalCores > 1 || !isHyperthreaded) {
                    coreInfo.bigCoreCount++;
                    coreInfo.remapTable.push_back({(uint8_t)coreNumber++, CoreType::Big});
                }
                else {
                    coreInfo.littleCoreCount++;
                    coreInfo.remapTable.push_back({(uint8_t)coreNumber++, CoreType::Little});
                }
                
                logicalCoreCount += logicalCores;
                break;
            }
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    coreInfo.isHyperthreaded = isHyperthreaded;
    coreInfo.physicalCoreCount = physicalCoreCount;
    
    #elif KRAM_ANDROID

    // TODO: have to walk array of proc/cpuinfo, and assemble this info, ugh
    // then build a core remap table since big core are typically last, little early
    // https://stackoverflow.com/questions/26239956/how-to-get-specific-information-of-an-android-device-from-proc-cpuinfo-file

    // JDK and NDK version of library with workarounds
    // https://github.com/google/cpu_features
    
    // hack - assume all big cores, typical 1/3/4 or 2/2/4
    coreInfo.bigCoreCount = coreInfo.physicalCoreCount;
    
    for (int32_t i = coreInfo.bigCoreCount-1; i >= 0; --i) {
        coreInfo.remapTable.push_back({(uint8_t)i, CoreType::Big});
    }
    
    #endif
    
    // sort faster cores first in the remap table
    sort(coreInfo.remapTable.begin(), coreInfo.remapTable.end(), [](const CoreNum& lhs, const CoreNum& rhs){
        if (lhs.type == rhs.type)
            return lhs.index > rhs.index;
        
        return lhs.type > rhs.type;
    });
    
    return coreInfo;
}


//------------------

#if KRAM_MAC || KRAM_IOS

void task_system::set_rr_priority(std::thread& thread, uint8_t priority)
{
    auto handle = thread.native_handle();
   
    struct sched_param param = { priority };
    pthread_setschedparam(handle, SCHED_RR, &param);
}

void task_system::set_main_rr_priority(uint8_t priority)
{
    auto handle = pthread_self();
   
    struct sched_param param = { priority };
    pthread_setschedparam(handle, SCHED_RR, &param);
}

void task_system::set_main_qos(ThreadQos level)
{
    set_qos(pthread_self(), level);
}

void task_system::set_qos(std::thread& thread, ThreadQos level)
{
    auto handle = thread.native_handle();
    set_qos(handle, level);
}

void task_system::set_qos(std::thread::native_handle_type handle, ThreadQos level)
{
    // https://abhimuralidharan.medium.com/understanding-threads-in-ios-5b8d7ab16f09
    // user-interactive, user-initiated, default, utility, background, unspecified
    
    qos_class_t qos = QOS_CLASS_UNSPECIFIED;
    switch(level) {
        case ThreadQos::Interactive: qos = QOS_CLASS_USER_INTERACTIVE; break;
        case ThreadQos::High: qos = QOS_CLASS_USER_INITIATED; break;
        case ThreadQos::Default: qos = QOS_CLASS_DEFAULT; break;
        case ThreadQos::Medium: qos = QOS_CLASS_UTILITY; break;
        case ThreadQos::Low: qos = QOS_CLASS_BACKGROUND; break;
    }
    
    // qos is transferred to GCD jobs, and can experience thread depriority
    // can system can try to adjust priority inversion.
    
    // note here the priorityOffset = 0, but is negative offsets
    // there is a narrow range of offsets
    
    // note this is a start/end overide call, but can set override on existing thread
    pthread_override_qos_class_start_np(handle, qos, 0);
}

#endif

void task_system::set_affinity(std::thread& thread, uint32_t threadIndex)
{
    // https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
    
    auto handle = thread.native_handle();
    
    set_affinity(handle, threadIndex);
}

void task_system::set_main_affinity(uint32_t threadIndex)
{
    set_affinity(pthread_self(), threadIndex);
}

void task_system::set_affinity(std::thread::native_handle_type handle, uint32_t threadIndex)
{
    const auto& coreInfo = GetCoreInfo();
    
    uint32_t maxIndex = coreInfo.remapTable.size() - 1;
    if (threadIndex > maxIndex)
        threadIndex = maxIndex;
    
    threadIndex = coreInfo.remapTable[threadIndex].index;
    
    // for now only allow single core mask
    uint64_t affinityMask = ((uint64_t)1) << threadIndex;
          
    // These are used in most of the paths
    macroUnusedVar(handle);
    macroUnusedVar(affinityMask);
    
#if KRAM_MAC
    #if KRAM_SSE
    if (!coreInfo.isTranslated) {
        thread_affinity_policy_data_t policy = { (int)affinityMask };

        // TODO: consider skipping affinity on macOS altogether
        // this is just a hint on x64-based macOS
        int returnVal = thread_policy_set(pthread_mach_thread_np(handle), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
        
        if (returnVal != 0) {
            // TODO: unsupported on iOS/M1, only have QoS and priority
            // big P cores can also be disabled to resolve thermals
        }
    }
    #endif
    
#elif KRAM_IOS
    // no support
    
#elif KRAM_ANDROID
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIndex, &cpuset);
    
    // convert pthread to pid
    pid_t pid;
    pthread_getunique_np(handle, &pid);
    if (!sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset)) {
        // TODO: this can fail on some/all cores
    }

#elif KRAM_WIN
    // each processor group only has 64 bits
    DWORD_PTR mask = SetThreadAffinityMask(handle, *(const DWORD_PTR*)&affinityMask);
    if (mask == 0) {
        // TODO: failure case
    }
#else
    // most systems are pthread-based, this is represented with array of bits
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIndex, &cpuset);

    // TODO: check return
    int returnVal = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
    if (returnVal != 0) {
        // TODO: linux pthread failure case
    }
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




task_system::task_system(int32_t count) :
    _count(std::min(count, (int32_t)GetCoreInfo().physicalCoreCount)),
    _q{(size_t)_count},
    _index(0)
{
#if KRAM_IOS || KRAM_MAC
        set_main_rr_priority(45);
#else
        set_main_affinity(0);
#endif
        
    // start up the threads
    for (int32_t threadIndex = 0; threadIndex != _count; ++threadIndex) {
        _threads.emplace_back([&, threadIndex] { run(threadIndex); });
        
#if KRAM_IOS || KRAM_MAC
        // it's either this or qos
        set_rr_priority(_threads.back(), 41);
#else
        set_affinity(_threads.back(), threadIndex);
#endif
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
