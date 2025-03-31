#include <queue>

namespace kram {
using namespace STL_NAMESPACE;

// this must not rollover
using AtomicValue = atomic<uint32_t>;

// fast locking
class futex {
    public: // for now leave this public
    AtomicValue _value;
    futex() = default;
public:
    void wait(uint32_t expectedValue);
    void notify_one();
    void notify_all();
};
    
// No affinity needed.  OS can shedule threads from p to e core.
// What about skipping HT though.
class Scheduler;

// This wraps priority and function together.
// A priority queue can then return higher priority jobs.
class Job2 {
public:
    int priority = 0; // smaller type?
    function<void()> job;
    
    Job2() {}
    Job2(int p, function<void()> f) : priority(p), job(f) {}
    
    bool operator<(const Job2& other) const {
        return priority > other.priority; // Higher priority comes first
    }
    
    void execute() { job(); }
};

class Worker {
public:
    string _name;
    
    priority_queue<Job2> _queue;
    mutex _mutex; // for queue
    futex _futex; // to wait/notify threads
    Scheduler* _scheduler = nullptr;
    uint32_t _workerId = 0;
    bool _isExecuting = false;
    
    void Init(const string& name, uint32_t workerId, Scheduler* scheduler) {
        _name = name;
        _workerId = workerId;
        _scheduler = scheduler;
    }
    
    void run();
    
private:
    bool stealFromOtherQueues(Job2& job);
    void wakeWorkers();
    bool shouldSleep();
};

struct SchedulerStats {
    AtomicValue jobsTotal;
    AtomicValue jobsExecuting;
    uint32_t jobsRemaining() const { return jobsTotal - jobsExecuting; }
};

class Scheduler {
public:
    Scheduler(uint32_t numWorkers);
    ~Scheduler() {
        if (!_isStop) {
            stop();
        }
    }

    void scheduleJob(Job2& job);
    
    bool isStop() const { return _isStop; }
    void stop();
    
    // Not really public API
    vector<Worker*>& workers() { return _workers; }
    
    SchedulerStats& stats() { return _stats; }
    
private:
    Worker* findWorker(thread::native_handle_type currentThread) {
        for (uint32_t i = 0; i < (uint32_t)_workers.size(); ++i) {
            if (_threads[i].native_handle() == currentThread) {
                return _workers[i];
            }
        }
        return nullptr;
    }
    
    bool _isStop = false;
    vector<Worker*> _workers;
    vector<thread> _threads;
    SchedulerStats _stats;
    thread::native_handle_type _schedulerThread = 0;
};

} // namespace kram
