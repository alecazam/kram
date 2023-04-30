#include "TaskSystem.h"

#if KRAM_MAC
    // affinity
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
    #include <processthreadsapi.h>
#elif KRAM_ANDROID
    #include <sys/resource.h>
#else
    #include <pthread/pthread.h>
#endif

// TODO: look at replacing this with Job Queue from Filament

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
#if KRAM_WIN
    uint8_t group; // for Win only
#endif
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
    
    // https://docs.microsoft.com/en-us/windows/win32/procthread/multiple-processors
    
    DWORD logicalCoreCount = 0;
    DWORD physicalCoreCount = 0;
    bool isHyperthreaded = false;
    
    using ProcInfo = SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;
    using ProcInfoPtr = PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;
    
    // get the exact size
    DWORD returnLength = 0;
    GetLogicalProcessorInformationEx(RelationAll, (ProcInfoPtr)nullptr, &returnLength);
    
    // This returns data on processor groupings
    vector<uint8_t> buffer;
    buffer.resize(returnLength);
    DWORD rc = GetLogicalProcessorInformationEx(RelationAll, (ProcInfoPtr)buffer.data(), &returnLength);
    
    ProcInfoPtr ptr = nullptr;
    DWORD byteOffset = 0;
    
    // walk the array
    ptr = (ProcInfoPtr)buffer.data();
    byteOffset = 0;
    while (byteOffset + sizeof(ProcInfo) <= returnLength) {
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
        
        byteOffset += sizeof(ProcInfo);
        ptr++;
    }
    
    ptr = (ProcInfoPtr)buffer.data();
    byteOffset = 0;
    
    uint8_t groupNumber = 0;
    uint32_t coreNumber = 0;
    while (byteOffset + sizeof(ProcInfo) <= returnLength) {
        switch (ptr->Relationship) {
            case RelationGroup:
                // Sounds like Win11 now allows all cores to be mapped.
                // TODO: groupNumber = ptr->activeGroup;
                break;
                
            case RelationProcessorCore: {
                physicalCoreCount++;
                
                // A hyperthreaded core supplies more than one logical processor.
                // Can identify AlderLake big vs. little off this
                uint32_t logicalCores = CountSetBits(ptr->ProcessorMask);
                if (logicalCores > 1 || !isHyperthreaded) {
                    coreInfo.bigCoreCount++;
                    coreInfo.remapTable.push_back({(uint8_t)coreNumber, (uint8_t)groupNumber, CoreType::Big});
                }
                else {
                    coreInfo.littleCoreCount++;
                    coreInfo.remapTable.push_back({(uint8_t)coreNumber, (uint8_t)groupNumber, CoreType::Little});
                }
                
                // Is this the correct index for physical cores?
                // Always go through remap table
                coreNumber += logicalCores;
                
                logicalCoreCount += logicalCores;
                break;
            }
        }
        byteOffset += sizeof(ProcInfo);
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
    std::sort(coreInfo.remapTable.begin(), coreInfo.remapTable.end(), [](const CoreNum& lhs, const CoreNum& rhs){
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

std::thread::native_handle_type getCurrentThread()
{
#if KRAM_WIN
    return ::GetCurrentThread();
#else
    return pthread_self();
#endif
}

// Ugh C++ "portable" thread classes that don't do anything useful
// and make you define all this over and over again in so many apps.
// https://stackoverflow.com/questions/10121560/stdthread-naming-your-thread
// Of course, Windows has to make portability difficult.
// And Mac non-standardly, doesn't even pass thread to call.
//   This requires it to be set from thread itself.

#if KRAM_WIN

// TODO: on Win, also need to set the following.  Then use Windows Termnial.
// SetConsoleOutputCP(CP_UTF8);

void setThreadName(std::thread::native_handle_type handle, const char* threadName)
{
    // TODO: use std::wstring_convert();
    // std::codecvt_utf8_utf16
    
    // ugh, win still using char16_t.  TODO: this isn't utf8 to utf16 conversion
    uint32_t len = strlen(threadName);
    std::wstring str;
    str.reserve(len);
    for (uint32_t i = 0; i < len; ++i) {
        if (threadName[i] <= 127)
            str.push_back((char)threadName[i]);
    }
    
    ::SetThreadDescription(handle, str.c_str());
}

void setCurrentThreadName(const char* threadName)
{
    setThreadName(getCurrentThread(), threadName);
}

void setThreadName(std::thread& thread, const char* threadName)
{
    setThreadName(thread.native_handle(), threadName);
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
    setThreadName(getCurrentThread(), threadName);
}

// This doesn't exist on macOS. What a pain.  Doesn't line up with getter calls.
// Means can't set threadName externally without telling thread to wake and set itself.
//void setThreadName(std::thread& thread, const char* threadName)
//{
//    auto handle = thread.native_handle();
//    setThreadName(handle, threadName);
//}

#else

// 15 char name limit on Linux/Android, how modern!
void setThreadName(std::thread::native_handle_type handle, const char* threadName)
{
    int val = pthread_setname_np(handle, threadName);
    if (val != 0)
        KLOGW("Thread", "Could not set thread name");
}

void setCurrentThreadName(const char* threadName)
{
    setThreadName(getCurrentThread(), threadName);
}

void setThreadName(std::thread& thread, const char* threadName)
{
    setThreadName(thread.native_handle(), threadName);
}

#endif

//------------------

#if KRAM_MAC || KRAM_IOS

static void setThreadPriority(std::thread::native_handle_type handle, ThreadPriority priority)
{
    if (priority == ThreadPriority::Default) {
        
        /* samples of qos
        qos_class_t qos = QOS_CLASS_UNSPECIFIED;
        switch(level) {
            case ThreadQos::Interactive: qos = QOS_CLASS_USER_INTERACTIVE; break;
            case ThreadQos::High: qos = QOS_CLASS_USER_INITIATED; break;
            case ThreadQos::Default: qos = QOS_CLASS_DEFAULT; break;
            case ThreadQos::Medium: qos = QOS_CLASS_UTILITY; break;
            case ThreadQos::Low: qos = QOS_CLASS_BACKGROUND; break;
        }
        */
        
        // qos is transferred to GCD jobs, and can experience thread depriority
        // can system can try to adjust priority inversion.
        
        // note here the priorityOffset = 0, but is negative offsets
        // there is a narrow range of offsets
        
        // note this is a start/end overide call, but can set override on existing thread
        // TODO: this returns a newly allocated object which isn't released here
        // need to release with pthread_override_qos_class_end_np(override);
        
        qos_class_t qos = QOS_CLASS_DEFAULT;
        auto val = pthread_override_qos_class_start_np(handle, qos, 0);
        if (val != nullptr)
            KLOGW("Thread", "Failed to set qos %d", (int)qos);
    }
    else {
        int prioritySys = 0;
        switch(priority) {
            case ThreadPriority::Default: prioritySys = 30;  break; // skipped above
            case ThreadPriority::High: prioritySys = 41; break;
            case ThreadPriority::Interactive: prioritySys = 45; break;
        }
                
        struct sched_param param = { prioritySys };
        
        // policy choices
        // SCHED_RR, SCHED_FIFO, SCHED_OTHER
        int policy = SCHED_RR;
        
        // this sets policy to round-robin and priority
        int val = pthread_setschedparam(handle, policy, &param);
        if (val != 0)
            KLOGW("Thread", "Failed to set policy %d priority %d", policy, prioritySys);
    }
}

#elif KRAM_ANDROID

static void setThreadPriority(std::thread::native_handle_type handle, uint8_t priority)
{
    // This doesn't change policy.
    // Android on -20 to 20, where lower is higher priority
    int prioritySys = 0;
    switch(priority) {
        case ThreadPriority::Default: prioritySys = 0;  break; // NORMAL
        case ThreadPriority::High: prioritySys = -4; break; // ABOVE NORMAL
        case ThreadPriority::Interactive: prioritySys = -8; break; // HIGHEST
    }
    
    int val = setpriority(PRIO_PROCESS, 0, prioritySys);
    if (val != 0)
        KLOGW("Thread", "Failed to set priority %d", prioritySys);
}

#elif KRAM_WIN

static void setThreadPriority(std::thread::native_handle_type handle, ThreadPriority priority)
{
    // This doesn't change policy.
    // Win has 0 to 15 normal, then 16-31 real time priority
    int prioritySys = 0;
    switch(priority) {
        case ThreadPriority::Default: prioritySys = 0;  break; // NORMAL
        case ThreadPriority::High: prioritySys = 1; break; // ABOVE NORMAL
        case ThreadPriority::Interactive: prioritySys = 2; break; // HIGHEST
    }
    
    BOOL success = SetThreadPriority(handle, prioritySys);
    if (!success)
        KLOGW("Thread", "Failed to set priority %d", prioritySys);
}

#endif

void task_system::set_current_priority(ThreadPriority priority)
{
    // Most systems can set priority from another thread, but Android can't
    setThreadPriority(getCurrentThread(), priority);
}

#if SUPPORT_AFFINITY

static void setThreadAffinity(std::thread::native_handle_type handle, uint32_t threadIndex)
{
    // https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
    //
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
    
    bool success = false;
    
#if KRAM_MAC || KRAM_IOS
    // no support, don't use thread_policy_set it's not on M1 and just a hint
    success = true;
    
#elif KRAM_ANDROID
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIndex, &cpuset);
    
    // convert pthread to pid
    pid_t pid;
    pthread_getunique_np(handle, &pid);
    success = sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) == 0;
    
#elif KRAM_WIN
    // each processor group only has 64 bits
    DWORD_PTR mask = SetThreadAffinityMask(handle, *(const DWORD_PTR*)&affinityMask);
    success = mask != 0;
    
#if 0 // TODO: finish this
    // Revisit Numa groups on Win, have 128-core/256 ThreadRipper
    // https://chrisgreendevelopmentblog.wordpress.com/2017/08/29/thread-pools-and-windows-processor-groups/
    
    // https://docs.microsoft.com/en-us/windows/win32/procthread/processor-groups
    // None of this runs on x86, only on x64.
    //
    // Starting with Windows 11 and Windows Server 2022, it is no longer the case that applications are constrained by default to a single processor group. Instead, processes and their threads have processor affinities that by default span all processors in the system, across multiple groups on machines with more than 64 processors.
    
    // win thread pool, but seems to limit to group 0
    //  https://github.com/stlab/libraries/blob/develop/stlab/concurrency/default_executor.hpp
    
    static int32_t threadIndexToGroup(int32_t threadIndex)
    {
        for (int32_t i = 0; i < nNumGroups; i++)
        {
            if (threadIndex < totalCores[i])
                return i;
        }
        return 0; // error
    }
    
    static void setupWinCoreGroups()
    {
        // Also have to test for HT on these, and fix remap table.
        // Table will need to be larger to accomodate.
        
        int32_t nNumGroups = GetActiveProcessorGroupCount();
        int32_t numCores[16] = {}; // TODO: make members
        int32_t totalCores[16] = 0;
        for (int32_t i = 0; i < nNumGroups; i++)
        {
            numCores[i] = GetMaximumProcessorCount(i);
            totalCores[i] += numCores[i];
        }
    }

    // have to adjust the mask for the core group
    int32_t groupNum = threadIndexToGroup(threadIndex);
    int32_t groupThreadIndex = (groupNum == 0) ? 0 : totalCores[groupNum-1];
    affinityMask = ((uint64_t)1) << (threadIndex - groupThreadIndex);

    // set group and affinity
    GROUP_AFFINITY affinity;
    affinity.group = groupNum;
    affinity.mask = *(const DWORD_PTR*)&affinityMask;
    success = SetThreadGroupAffinity(hndl, &affinity, nullptr);
    
    // also this hint to scheduler (for > 64 cores)
    // SetThreadIdealProcessorEx( );
#endif
    
#else
    // most systems are pthread-based, this is represented with array of bits
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIndex, &cpuset);

    // TODO: check return
    success = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset) == 0;
#endif
    if (!success)
        KLOGW("Thread", "Failed to set affinity");
}

void task_system::set_current_affinity(uint32_t threadIndex)
{
    setThreadAffinity(getCurrentThread(), threadIndex);
}


#endif

void task_system::run(int32_t threadIndex)
{
    while (true) {
        // pop() wait avoids a spinloop.

        myfunction<void()> f;

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

struct ThreadInfo {
    const char* name = "";
    ThreadPriority priority = ThreadPriority::Default;
    int affinity = 0; // single core for now
};

// This only works for current thread, but simplifies setting several thread params.
void setThreadInfo(ThreadInfo& info) {
    setCurrentThreadName(info.name);

    setThreadPriority(getCurrentThread(), info.priority);
    
    #if SUPPORT_AFFINITY
    setThreadAffinity(getCurrentThread(), info.affinity);
    #endif
}

task_system::task_system(int32_t count) :
    _count(std::min(count, (int32_t)GetCoreInfo().physicalCoreCount)),
    _q{(size_t)_count},
    _index(0)
{
    // see WWDC 2021 presentation here
    // Tune CPU job scheduling for Apple silicon games
    // https://developer.apple.com/videos/play/tech-talks/110147/
    ThreadInfo infoMain = { "Main", ThreadPriority::Interactive, 0 };
    setThreadInfo(infoMain);
    
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
            ThreadInfo infoTask = { name.c_str(), ThreadPriority::High, threadIndex };
            setThreadInfo(infoTask);

            run(threadIndex);
        });
    }
        
    // dump out thread data
    log_threads();
}

static ThreadPriority getThreadPriority(std::thread::native_handle_type handle)
{
    ThreadPriority priority = ThreadPriority::Default;
    
#if KRAM_MAC || KRAM_IOS || KRAM_ANDROID
    // Note: this doesn't handle qOS, and returns default priority
    // on those threads.
    
    int policy = 0;
    struct sched_param priorityVal;
    int val = pthread_getschedparam(handle, &policy, &priorityVal);
    if (val != 0)
        KLOGW("Thread", "failed to retrieve thread data");
    int prioritySys = priorityVal.sched_priority;
    
    // remap back to enum
    switch(prioritySys) {
        case 41: priority = ThreadPriority::High; break;
        case 45: priority = ThreadPriority::Interactive; break;
        default: priority = ThreadPriority::Default; break;
    }
    
/* Using code above since it may work with other threads
#elif KRAM_ANDROID
    // Note: only for current thread
    
    // only have getpriority call on current thread
    // pthread_getschedparam never returns valid data
    int priority = getpriority(PRIO_PROCESS, 0);
    switch(prioritySys) {
        case 41: priority = ThreadPriority::High; break;
        case 45: priority = ThreadPriority::Interactive; break;
        default: priority = ThreadPriority::Default; break;
    }
*/
#elif KRAM_WIN
    // all threads same policy on Win?
    // https://www.microsoftpressstore.com/articles/article.aspx?p=2233328&seqNum=7#:~:text=Windows%20never%20adjusts%20the%20priority,the%20process%20that%20created%20it.
    
    // scheduling based on process priority class, thread priority is +/- offset
    // DWORD priorityClass = GetPriorityClass(GetCurrentProcess());
    
    // The handle must have the THREAD_QUERY_INFORMATION or THREAD_QUERY_LIMITED_INFORMATION access right.
    int prioritySys = GetThreadPriority(handle);
    if (prioritySys == THREAD_PRIORITY_ERROR_RETURN)
        prioritySys = 0;
    
    switch(prioritySys) {
        case 1: priority = ThreadPriority::High; break;
        case 2: priority = ThreadPriority::Interactive; break;
        default: priority = ThreadPriority::Default; break;
    }
#endif
    
    return priority;
}


void task_system::log_threads()
{
    ThreadInfo info = {};
    info.name = "Main";
#if SUPPORT_AFFINITY
    info.affinity = 0;
#endif
    
    info.priority = getThreadPriority(getCurrentThread());
    KLOGI("Thread", "Thread:%s (pri:%d aff:%d)",
          info.name, info.priority, info.affinity);
    
    for (uint32_t i = 0; i < _threads.size(); ++i)
    {
        info.name = _threadNames[i].c_str();
#if SUPPORT_AFFINITY
        // TODO: if more tasks/threads than cores, then this isn't accurate
        // but don't want to write a getter for this right now.
        info.affinity = i;
#endif
        info.priority = getThreadPriority(_threads[i].native_handle());
        KLOGI("Thread", "Thread:%s (pri:%d aff:%d)",
              info.name, info.priority, info.affinity);
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
