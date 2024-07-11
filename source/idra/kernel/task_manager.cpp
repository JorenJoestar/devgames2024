#include "kernel/task_manager.hpp"

// Note: commenting this will lock the task manager in shutdown method.
// #define TASK_MANAGER_LOG

namespace idra {

void TaskManager::run_task( i32 thread_id ) {
    while ( true ) {

#if defined(TASK_MANAGER_LOG)
        CRB_DBG( "Waiting\n" );
#endif // TASK_MANAGER_LOG

        {
            auto lck = std::unique_lock<std::mutex>( tasks_mtx );
            tasks_available_cv.wait( lck, [ this ]() {
                return tasks_available.load();
                                     } );
        }

#if defined(TASK_MANAGER_LOG)
        CRB_DBG( "Waking up\n" );
#endif // TASK_MANAGER_LOG

        int next_task = next_task_indices.fetch_add( 1 );
        while ( next_task < task_queue.size() ) {
            Task& t = task_queue[ next_task ];

#if defined(TASK_MANAGER_LOG)
        //CRB_DBG( "Executing task %d\n", next_task );
#endif // TASK_MANAGER_LOG

            t.fn( t.data );

            next_task = next_task_indices.fetch_add( 1 );
            tasks_completed_count.fetch_add( 1 );
        }

        tasks_completed_cv.notify_one();

        if ( active.load() ) {
            // NOTE(marco): only set this when the task manager is active, otherwise the shutdown method
            // won't work, as the threads that get woken up will see this as false and go back to wait
            auto lck = std::unique_lock<std::mutex>( tasks_mtx );
            tasks_available.store( false );
        } else {
#if defined(TASK_MANAGER_LOG)
            CRB_DBG( "Stopping thread\n" );
#endif // TASK_MANAGER_LOG

            break;
        }
    }
}

void TaskManager::init() {
    // NOTE(marco): leave room for main thread, physics thread and audio thread
    const int num_threads = std::thread::hardware_concurrency() - 3;

    for ( int i = 0; i < num_threads; ++i ) {
        thread_pool.push_back(
            std::thread( &TaskManager::run_task, this, i )
        );
    }

    {
        std::lock_guard<std::mutex> lck( tasks_mtx );
        active.store( true );
    }
}

void TaskManager::shutdown() {
#if defined(TASK_MANAGER_LOG)
    CRB_DBG( "Shutting down\n" );
#endif

    {
        std::lock_guard<std::mutex> lck( tasks_mtx );
        tasks_available.store( true );
        active.store( false );
    }

    tasks_available_cv.notify_all();

    for ( std::thread& t : thread_pool ) {
        if ( t.joinable() )
            t.join();
    }

    thread_pool.clear();
}

i32 TaskManager::add_task( Callback& fn, void* data ) {
    i32 task_id = (i32)task_queue.size();

    Task t{
        task_id,
        data,
        fn
    };
    task_queue.push_back( t );

    return task_id;
}

void TaskManager::start_tasks() {
    tasks_completed_count = 0;
    next_task_indices = 0;
    {
        std::lock_guard<std::mutex> lck( tasks_mtx );
        tasks_available.store( true );
    }

    tasks_available_cv.notify_all();
}

void TaskManager::wait_for_completion() {
    int s = task_queue.size();

#if defined(TASK_MANAGER_LOG)
    CRB_DBG( "Wait for completion\n" );
#endif

    {
        auto lck = std::unique_lock<std::mutex>( tasks_mtx );

        tasks_completed_cv.wait( lck, [ this, s ]() {
            return tasks_completed_count >= s;
                                 } );
    }

#if defined(TASK_MANAGER_LOG)
    CRB_DBG( "Completed\n" );
#endif

    {
        std::lock_guard<std::mutex> lck( tasks_mtx );
        task_queue.clear();
    }
}
    
} // namespace idra
