#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <future>

#include "kernel/platform.hpp"

namespace idra {

struct TaskManager {
	
    using Callback = std::function<void( void* data )>;

    struct Task {
        i32                 id;
        void*               data;
        Callback            fn;
    };

    void                    init();
    void                    shutdown();

    void                    run_task( i32 thread_id );
    i32                     add_task( Callback& fn, void* data );
    void                    start_tasks();

    void                    wait_for_completion();

    std::mutex              tasks_mtx;
    std::atomic_bool        tasks_available;
    std::condition_variable tasks_available_cv;

    std::condition_variable tasks_completed_cv;

    std::atomic_bool        active;

    std::atomic_int         tasks_completed_count;
    std::atomic_int         next_task_indices;

    std::vector<Task>       task_queue;
    std::vector<std::thread> thread_pool;
};
} // namespace idra
