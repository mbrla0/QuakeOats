#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

class semaphore {
private:
    std::uint32_t permits;
    std::mutex lock;
    std::condition_variable cond;
public:
    semaphore(std::uint32_t initial_permits): permits(initial_permits) {}

    void acquire() noexcept {
        std::unique_lock<std::mutex> l(lock);
        while(permits == 0) {
            cond.wait(l);
        }
        permits--;
    }

    void release() noexcept {
        std::lock_guard<std::mutex> l(lock);
        permits++;
        cond.notify_one();
    }

    bool try_acquire() noexcept {
        std::lock_guard<std::mutex> l(lock);
        if(permits > 0) {
            permits--;
            return true;
        }
        return false;
    }
};

template<typename T>
class locked_queue {
private:
    std::queue<T> q;
    std::mutex lock;
    std::condition_variable cond;
public:
    explicit locked_queue() {}

    void enqueue(T t) noexcept {
        std::lock_guard<std::mutex> l(lock);
        q.push(t);
        cond.notify_one();
    }

    T dequeue() noexcept {
        std::unique_lock<std::mutex> l(lock);
        while(q.empty()) {
            cond.wait(l);
        }
        auto r = q.front();
        q.pop();
        return r;
    }

    bool try_dequeue(T& ref) noexcept {
        std::lock_guard<std::mutex> l(lock);
        if(q.empty()) {
            return false;
        }
        ref = q.front();
        q.pop();
        return true;
    }
};

using WorkerTask = std::function<bool(std::uint32_t)>;
using Task = std::function<void(std::uint32_t)>;

class thread_pool;
class thread_pool_worker {
    friend class thread_pool;
private:
    std::uint32_t id;
    locked_queue<WorkerTask> external_tasks;
    std::queue<WorkerTask> local_tasks;
    std::thread thr;
    std::promise<std::thread::id> thread_id_promise;

    void start() {
        thr = std::thread(&thread_pool_worker::run, this);
    }
    void queue_stop() {
        queue_task([](std::uint32_t _ignored) { return false; });
    }

    void run() {
        thread_id_promise.set_value(std::this_thread::get_id());
        while(1) {
            auto t = get_next_task();
            if(!t(id)) break;
        }
    }
    WorkerTask get_next_task() {
        if(!local_tasks.empty()) {
            auto t = local_tasks.front();
            local_tasks.pop();
            return t;
        }
        return external_tasks.dequeue();
    }

    void queue_local_task(WorkerTask t) {
        local_tasks.push(t);
    }
public:
    explicit thread_pool_worker(std::uint32_t _id): id(_id) {}

    void queue_task(WorkerTask t) {
        external_tasks.enqueue(t);
    }
};

class thread_pool {
private:
    std::uint32_t worker_count;
    std::atomic<std::uint32_t> next_worker;
    std::vector<thread_pool_worker*> workers;
    std::map<std::thread::id, std::uint32_t> worker_ids;

    std::uint32_t get_next_worker() {
        auto id = next_worker.fetch_add(1, std::memory_order_acq_rel) % worker_count;
        return id;
    }
public:
    explicit thread_pool(std::uint32_t size): worker_count(size) {
        workers.reserve(size);
        for(std::uint32_t i = 0; i < size; i++) {
            auto w = new thread_pool_worker(i);
            workers.push_back(w);
            w->start();
        }
        for(std::uint32_t i = 0; i < size; i++) {
            auto future = workers[i]->thread_id_promise.get_future();
            future.wait();
            const auto id = future.get();
            worker_ids.insert({id, i});
        }
    }
    ~thread_pool() {
        for(auto& w : workers) {
            w->queue_stop();
        }
        for(auto& w : workers) {
            w->thr.join();
            delete w;
        }
    }

    void submit_task_for(std::uint32_t tid, Task t) {
        workers[tid]->queue_task([t](std::uint32_t id) { t(id); return true; });
    }

    void submit_task(Task t) {
        if(auto tid = current_tid()) {
            workers[*tid]->queue_local_task([t](std::uint32_t id) {
                t(id);
                return true;
            });
            return;
        }
        submit_task_for(get_next_worker(), t);
    }

    std::optional<std::uint32_t> current_tid() const noexcept {
        const auto& it = worker_ids.find(std::this_thread::get_id());
        if(it != worker_ids.end()) {
            return std::optional<std::uint32_t> { it->second };
        } else {
            return std::nullopt;
        }
    }
};

