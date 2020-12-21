#pragma once

#include <atomic>               //std::atomic, std::memory_order_*
#include <condition_variable>   //std::condition_variable
#include <cstdint>              //std::uint32_t
#include <deque>                //std::deque
#include <functional>           //std::function
#include <future>               //std::future, std::packaged_task
#include <initializer_list>     //std::initializer_list
#include <map>                  //std::map
#include <memory>               //std::unique_ptr, std::make_unique
#include <mutex>                //std::mutex
#include <optional>             //std::optional
#include <thread>               //std::thread
#include <vector>               //std::vector

template<typename T>
class work_queue {
private:
    std::deque<T> q;
    std::mutex lock;
    std::condition_variable cond;

    //prevent copying
    work_queue(const work_queue&) = delete;
    work_queue& operator=(const work_queue&) = delete;
public:
    explicit work_queue() {}

    /**
     * Inserts a value into the queue. The value is moved into the queue, rather than copied.
     */
    void enqueue(T&& t) noexcept {
        std::lock_guard<std::mutex> l(lock);
        q.push_back(std::move(t));
        cond.notify_one();
    }

    /**
     * Removes a value from the queue.
     */
    T dequeue() noexcept {
        std::unique_lock<std::mutex> l(lock);
        while(q.empty()) {
            cond.wait(l);
        }
        auto r = std::move(q.front());
        q.pop_front();
        return r;
    }

    /**
     * Attempts to remove a value from the queue, returning whether or not
     * the removal was successul. If removal fails, the reference provided
     * is unmodified.
     */
    bool try_dequeue(T& ref) noexcept {
        std::lock_guard<std::mutex> l(lock);
        if(q.empty()) {
            return false;
        }
        ref = std::move(q.front());
        q.pop_front();
        return true;
    }

    /**
     * Attempts to steal a value from the queue, returning whether or not
     * the removal was successful. If removal fails, the reference provided 
     * is unmodified.
     *
     * The difference between this method and `try_dequeue` is that this method
     * removes from the back of the queue instead of the front.
     */
    bool try_steal(T& ref) noexcept {
        std::lock_guard<std::mutex> l(lock);
        if(q.empty()) {
            return false;
        }
        ref = std::move(q.back());
        q.pop_back();
        return true;
    }
};

using WorkerTask = std::packaged_task<void(std::uint32_t)>;
template<typename T>
using Task = std::function<T(std::uint32_t)>;

/**
 * Creates a task from a function which takes no arguments, ignoring the
 * provided thread id.
 */
template<typename T>
Task<T> make_task(std::function<T()> f) {
    return [f](std::uint32_t id) {
        (void)id;
        return f();
    };
}

class thread_pool;
class thread_pool_worker {
private:
    friend class thread_pool;

    const std::uint32_t id;
    work_queue<WorkerTask> external_tasks;
    std::deque<WorkerTask> local_tasks;
    std::thread thr;
    std::promise<std::thread::id> thread_id_promise;

    //prevent copying
    thread_pool_worker(const thread_pool_worker&) = delete;
    thread_pool_worker& operator=(const thread_pool_worker&) = delete;
 
    void run() {
        thread_id_promise.set_value(std::this_thread::get_id());
        while(1) {
            auto t = get_next_task();
            if(!t.valid()) break;
            t(id);
        }
    }
    WorkerTask get_next_task() {
        if(!local_tasks.empty()) {
            auto t = std::move(local_tasks.front());
            local_tasks.pop_front();
            return t;
        }
        return external_tasks.dequeue();
    }

    void queue_local_task(WorkerTask&& t) {
        local_tasks.push_back(std::move(t));
    }
    
    void queue_task(WorkerTask&& t) {
        external_tasks.enqueue(std::move(t));
    }
public:
    explicit thread_pool_worker(std::uint32_t _id): id(_id) {
        thr = std::thread(&thread_pool_worker::run, this);
    }

    ~thread_pool_worker() {
        queue_task({});
        thr.join();
    }
};

class thread_pool {
private:
    std::uint32_t worker_count;
    std::atomic<std::uint32_t> next_worker;
    std::vector<std::unique_ptr<thread_pool_worker>> workers;
    std::map<std::thread::id, std::uint32_t> worker_ids;

    //prevent copying
    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    
    std::uint32_t get_next_worker() {
        auto id = next_worker.fetch_add(1, std::memory_order_acq_rel) % worker_count;
        return id;
    }

public:
    explicit thread_pool(std::uint32_t size): worker_count(size) {
        next_worker.store(0);
        workers.reserve(size);
        for(std::uint32_t i = 0; i < size; i++) {
            workers.push_back(std::make_unique<thread_pool_worker>(i));
        }
        for(std::uint32_t i = 0; i < size; i++) {
            auto future = workers[i]->thread_id_promise.get_future();
            future.wait();
            const auto id = future.get();
            worker_ids.insert({id, i});
        }
    }

    /**
     * Returns the default concurrency for a thread pool. This is the current
     * system's hardware concurrency, defaulting to 4 if that can't be read.
     */
    static std::uint32_t default_concurrency() noexcept {
        auto size = std::thread::hardware_concurrency();
        if(size < 1) {
            size = 4;
        }
        return size;
    }

    /**
     * Creates a new thread pool with the default concurrency.
     */
    static thread_pool create() noexcept {
        return thread_pool(default_concurrency());
    }

    /**
     * Returns the number of threads in this pool.
     */
    std::uint32_t size() const noexcept { return worker_count; }

    /**
     * Submits a task to a given thread. The provided thread number must be
     * in the range [0, thread_count).
     */
    template<typename T>
    std::future<T> submit_task_for(std::uint32_t tid, Task<T> t) noexcept {
        auto p = std::packaged_task<T(std::uint32_t)>(t);
        auto f = p.get_future();
        workers[tid]->queue_task(std::packaged_task<void(std::uint32_t)>(std::move(p)));
        return f;
    }

    /**
     * Submits a task to the pool. If the caller is already running in one of
     * the pool's threads, the task is added to a local queue, which means it'll
     * run on the same thread, and *cannot* be stolen by another thread.
     *
     * If the `allow_local` argument is set to `false`, the current thread's local queue
     * is ignored, and the task is submitted to any of the pool's threads.
     */
    template<typename T>
    std::future<T> submit_task(Task<T> t, bool allow_local = true) noexcept {
        if(allow_local) {
            if(auto tid = current_tid()) {
                auto p = std::packaged_task<T(std::uint32_t)>(t);
                auto f = p.get_future();
                workers[*tid]->queue_local_task(std::packaged_task<void(std::uint32_t)>(std::move(p)));
                return f;
            }
        }
        return submit_task_for<T>(get_next_worker(), t);
    }

    /**
     * Convenience method to submit multiple tasks to the pool, spreading them through all
     * workers.
     */
    template<typename T>
    std::vector<std::future<T>> submit_all(std::initializer_list<Task<T>> tasks) noexcept {
        std::vector<std::future<T>> res;
        res.reserve(tasks.size());
        //if inside a pool, optimize by adding to the local queue if possible,
        //also starting in the current thread to bias towards the local queue
        //to avoid synchronization overhead (assuming a pool of 4 threads, this
        //would mean the current thread gets 2 tasks if 5 are given to this method)
        if(auto id = current_tid()) {
            std::uint32_t current = *id;
            std::uint32_t thread = current;
            for(auto t : tasks) {
                res.emplace_back(submit_task(t, thread == current));
                thread = (thread + 1) % size();
            }
        } else {
            for(auto t : tasks) {
                res.emplace_back(submit_task(t, false));
            }
        }
        return res;
    }

    /**
     * Returns the ID of the current thread, if running inside the pool.
     */
    std::optional<std::uint32_t> current_tid() const noexcept {
        const auto& it = worker_ids.find(std::this_thread::get_id());
        if(it != worker_ids.end()) {
            return std::optional<std::uint32_t> { it->second };
        } else {
            return std::nullopt;
        }
    }
};

