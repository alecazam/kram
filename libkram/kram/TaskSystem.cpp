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
    bool isHyperthreaded = false;
    
    DWORD returnLength = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
    DWORD rc = GetLogicalProcessorInformation(buffer, &returnLength);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = nullptr;
    DWORD byteOffset = 0;
    
    // walk the array
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
#if KRAM_ANDROID
        // sort largest index
        if (lhs.type == rhs.type)
            return lhs.index > rhs.index;
        return lhs.type > rhs.type;
#else
        // sort smallest index
        if (lhs.type == rhs.type)
            return lhs.index < rhs.index;
        return lhs.type > rhs.type;
#endif
        
       
    });
    
    return coreInfo;
}

//----------------------

// Ugh C++ "portable" thread classes that don't do anything useful
// and make you define all this over and over again in so many apps.
// https://stackoverflow.com/questions/10121560/stdthread-naming-your-thread
// Of course, Windows has to make portability difficult.
// And Mac non-standardly, doesn't even pass thread to call.
//   This requires it to be set from thread itself).

#if KRAM_WIN

// Isn't this in a header?
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void setThreadName(std::thread::native_handle_type handle, const char* threadName)
{
   DWORD threadID = ::GetThreadId(handle);

   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = threadID;
   info.dwFlags = 0;

   __try
   {
       // Limits to how long this name can be.  Also copy into ptr to change name.
      RaiseException(0x406D1388, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
}

void setCurrentThreadName(const char* threadName)
{
    setThreadName(GetCurrentThread(), threadName);
}

void setThreadName(std::thread& thread, const char* threadName)
{
    DWORD threadId = ::GetThreadId(thread.native_handle());
    setThreadName(threadId, threadName);
}

#elif KRAM_MAC || KRAM_IOS

void setThreadName(std::thread::native_handle_type macroUnusedArg(handle), const char* threadName)
{
    // This can only set on self
    int val = pthread_setname_np(threadName);
    if (val != 0)
        KLOGW("Thread", "Could not set thread name");
}

void setCurrentThreadName(const char* threadName)
{
    auto handle = pthread_self();
    setThreadName(handle, threadName);
}

// This doesn't exist on macOS. What a pain.  Doesn't line up with getter calls.
// Means can't set threadName externally without telling thread to wake and set itself.
//void setThreadName(std::thread& thread, const char* threadName)
//{
//    auto handle = thread.native_handle();
//    setThreadName(handle, threadName);
//}

#else

void setThreadName(std::thread::native_handle_type handle, const char* threadName)
{
    // This can only set on self
    int val = pthread_setname_np(handle, threadName);
    if (val != 0)
        KLOGW("Thread", "Could not set thread name");
}

void setCurrentThreadName(const char* threadName)
{
    auto handle = pthread_self();
    setThreadName(handle, threadName);
}

void setThreadName(std::thread& thread, const char* threadName)
{
    auto handle = thread.native_handle();
    setThreadName(handle, threadName);
}

#endif

//------------------

#if SUPPORT_PRIORITY
#if KRAM_MAC || KRAM_IOS

static void setThreadPriority(std::thread::native_handle_type handle, uint8_t priority)
{
    struct sched_param param = { priority };

    // this sets policy to round-robin and priority
    int val = pthread_setschedparam(handle, SCHED_RR, &param);
    if (val != 0)
        KLOGW("Thread", "Failed to set priority %d", priority);
}

static void setThreadQos(std::thread::native_handle_type handle, ThreadQos level)
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
    // TODO: this returns a newly allocated object which isn't released here
    // need to release with pthread_override_qos_class_end_np(override);
    auto val = pthread_override_qos_class_start_np(handle, qos, 0);
    if (val != nullptr)
        KLOGW("Thread", "Failed to set qos %d", (int)qos);
}

void task_system::set_priority(std::thread& thread, uint8_t priority)
{
    setThreadPriority(thread.native_handle(), priority);
}

void task_system::set_current_priority(uint8_t priority)
{
    setThreadPriority(pthread_self(), priority);
}

void task_system::set_current_qos(ThreadQos level)
{
    setThreadQos(pthread_self(), level);
}

void task_system::set_qos(std::thread& thread, ThreadQos level)
{
    auto handle = thread.native_handle();
    setThreadQos(handle, level);
}



#elif KRAM_ANDROID

void setThreadPriority(std::thread::native_handle_type handle, uint8_t priority)
{
    struct sched_param param = { priority };
    
    // Android doesn not allow policy change (prob SCHED_OTHER), and only allows setting priority;
    // Only from Android 10 (API 28).
    int val = pthread_setschedprio(handle, priority);
    if (val != 0)
        KLOGW("Thread", "Failed to set priority %d", priority);
}


static uint8_t convertQosToPriority(ThreadQos level)
{
    // TODO: fix these priorities.  Linux had 20 to -20 as priorities
    // but unclear what Android wants set from the docs.
    uint8_t priority = 30;
    switch(level) {
        case ThreadQos::Interactive: priority = 45; break;
        case ThreadQos::High: priority = 41; break;
        case ThreadQos::Default: priority = 31; break;
        case ThreadQos::Medium: priority = 20; break;
        case ThreadQos::Low: priority = 10; break;
    }
    return priority;
}

void task_system::set_priority(std::thread& thread, uint8_t priority)
{
    setThreadPriority(thread.native_handle(), priority);
}

void task_system::set_current_priority(uint8_t priority)
{
    setThreadPriority(pthread_self(), priority);
}


void task_system::set_main_qos(ThreadQos level)
{
    uint8_t priority = convertQosToPriority(level);
    set_current_priority(priority);
}

void task_system::set_qos(std::thread& thread, ThreadQos level)
{
    uint8_t priority = convertQosToPriority(level);
    set_priority(thread, priority);
}
#endif
#endif

#if SUPPORT_AFFINITY

static void setThreadAffinity(std::thread::native_handle_type handle, uint32_t threadIndex)
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
    // don't use this, it's unsupported on ARM chips, and only an affinity hints on x64
//    #if KRAM_SSE
//    if (!coreInfo.isTranslated) {
//        thread_affinity_policy_data_t policy = { (int)affinityMask };
//
//        // TODO: consider skipping affinity on macOS altogether
//        // this is just a hint on x64-based macOS
//        int returnVal = thread_policy_set(pthread_mach_thread_np(handle), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
//
//        if (returnVal != 0) {
//            // TODO: unsupported on iOS/M1, only have QoS and priority
//            // big P cores can also be disabled to resolve thermal throttling
//        }
//    }
//    #endif
    
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

void task_system::set_affinity(std::thread& thread, uint32_t threadIndex)
{
    // https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
    auto handle = thread.native_handle();
    setThreadAffinity(handle, threadIndex);
}

void task_system::set_main_affinity(uint32_t threadIndex)
{
#if KRAM_WIN
    setThreadAffinity(::GetCurrentThread(), threadIndex);
#else
    setThreadAffinity(pthread_self(), threadIndex);
#endif
}


#endif

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
    // see WWDC 2021 presentation here
    // Tune CPU job scheduling for Apple silicon games
    // https://developer.apple.com/videos/play/tech-talks/110147/
#if SUPPORT_PRIORITY
    set_current_priority(45);
#endif
        
#if SUPPORT_AFFINITY
    set_current_affinity(0);
#endif
        
    setCurrentThreadName("Main");
        
    // Note that running work on core0 when core0 may starve it
    // from assigning work to threads.
        
    // start up the threads
    string name;
    for (int32_t threadIndex = 0; threadIndex != _count; ++threadIndex) {
        
        // Generate a name, also corresponds to core for affinity
        // May want to include priority too.
        sprintf(name, "Task%d", threadIndex);
        _threadNames.push_back(name);
        
        _threads.emplace_back([&, threadIndex, name] {
            // Have to set name from thread only for Apple.
            setCurrentThreadName(name.c_str());
            
            run(threadIndex);
        });
        
#if SUPPORT_PRIORITY
        // it's either this or qos
        set_priority(_threads.back(), 41);
#endif
        
#if SUPPORT_AFFINITY
        set_affinity(_threads.back(), threadIndex);
#endif
    }
        
    // dump out thread data
    log_threads();
}

struct ThreadInfo {
    const char* name;
    int policy;
    int priority;
    int affinity; // single core for now
};

static void getThreadInfo(std::thread::native_handle_type handle, int& policy, int& priority)
{
#if KRAM_MAC || KRAM_IOS || KRAM_ANDROID
    struct sched_param priorityVal;
    int val = pthread_getschedparam(handle, &policy, &priorityVal);
    if (val != 0)
        KLOGW("Thread", "failed to retrieve thread data");
    priority = priorityVal.sched_priority;
#endif
}


void task_system::log_threads()
{
    ThreadInfo info = {};
    info.name = "Main";
#if SUPPORT_AFFINITY
    info.affinity = 0;
#endif
    
    getThreadInfo(pthread_self(), info.policy, info.priority);
    KLOGI("Thread", "Thread:%s (pol:%d pri:%d aff:%d)",
          info.name, info.policy, info.priority, info.affinity);
    
    for (uint32_t i = 0; i < _threads.size(); ++i)
    {
        info.policy = 0;
        info.priority = 0;
        info.name = _threadNames[i].c_str();
#if SUPPORT_AFFINITY
        // TODO: if more tasks/threads than cores, then this isn't accurate
        // but don't want to write a getter for this right now.
        info.affinity = i;
#endif
        getThreadInfo(_threads[i].native_handle(), info.policy, info.priority);
        KLOGI("Thread", "Thread:%s (pol:%d pri:%d aff:%d)",
              info.name, info.policy, info.priority, info.affinity);
    }
}

task_system::~task_system()
{
    // indicate that all tasks are submitted
    for (auto& e : _q)
        e.set_done();

    // wait until threads are all done by joining each thread
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
