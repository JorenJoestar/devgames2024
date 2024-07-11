/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include <thread>
#include <atomic>
#include <future>

#include "kernel/string_view.hpp"

namespace idra {

// First possible implementation of a thread.
    // Uses inheritance to execute things.
struct Thread {

    Thread( StringView name );
    ~Thread();

    Thread() = default;
    Thread( Thread const& other ) = delete;
    Thread( Thread&& other ) = default;

    Thread&             operator=( Thread const& other ) = delete;
    Thread&             operator=( Thread&& other ) = default;

    virtual void        run( std::future<void> const& stop_token ) = 0;

    void                start();
    void                stop();
    void                join();

    static bool         is_stop_requested( std::future<void> const& token ) noexcept;

    std::thread         thread;
    std::promise<void>  stop_request;
    StringView          name;

}; // struct Thread

//
//
struct ThreadLambda {

    ThreadLambda( StringView name );
    ThreadLambda( const ThreadLambda& other );
    ~ThreadLambda();
    

    void                start( std::function<void( ThreadLambda& )> threadFunc );
    void                stop();
    void                join();
    
    static bool         is_stop_requested( std::future<void> const& token ) noexcept;

    std::thread         thread;
    std::atomic<bool>   running;
    StringView          name;

}; // struct ThreadLambda

} // namespace idra
