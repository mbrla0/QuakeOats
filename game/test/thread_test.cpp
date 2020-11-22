//compile with `c++ -I"." -lpthread -std=c++17 test/thread_test.cpp`
#include "thread_utils.hpp"
#include <iostream>
#include <chrono>

std::mutex io_mutex;
template<typename... Args>
void locked_print(std::ostream& s, Args&&... args) {
    std::unique_lock<std::mutex> lock(io_mutex);
    (s << ... << args);
}

void sleepmp(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int main() {
    //create pool with default concurrency
    auto p = thread_pool::create();
    if(p.size() < 4) {
        std::cerr << "This test requires at least 4 threads" << std::endl;
        return 1;
    }

    //submit a task to a specific thread
    p.submit_task_for<void>(3, [&p](std::uint32_t id) {
        locked_print(std::cout, "Running in thread ", id, "\n");
        //this task is added to the current worker's local queue, which can
        //be accessed with no synchronization overhead (aka is faster to access)
        p.submit_task<void>([id](std::uint32_t id2) {
            locked_print(std::cout, "Now running in thread ", id2, " (should be ", id, ")\n");
        });
    });
    {
        //submitting a task returns a future to read it's result
        auto f = p.submit_task<int>([](std::uint32_t id2) {
            locked_print(std::cout, "Third task is in ", id2, "\n");
            return 69;
        });
        locked_print(std::cout, "Future result = ", f.get(), "\n");
    }
    {
        std::vector<std::future<int>> v;
        //sleep to simulate long tasks, f.get() should be called only after
        //all tasks are queued, so they actually run in parallel.
        auto begin = std::chrono::steady_clock::now();
        for(auto i = 0u; i < p.size(); i++) {
            v.push_back(p.submit_task<int>([i](std::uint32_t id) {
                (void)id;
                sleepmp(1000);
                return i;
            }));
        }
        for(auto& f : v) {
            locked_print(std::cout, "Sleep future result = ", f.get(), "\n");
        }
        auto end = std::chrono::steady_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        locked_print(std::cout, "Time elapsed = ", time, "ms\n");
        if(time > 1500) {
            locked_print(std::cerr, "Took too long!\n");
            return 1;
        }
    }
}