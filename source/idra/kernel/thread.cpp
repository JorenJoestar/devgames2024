/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "kernel/thread.hpp"

#include "kernel/allocator.hpp"
#include "kernel/memory.hpp"
#include "kernel/utf.hpp"

#if defined (_MSC_VER)
#include <windows.h>
#endif // _MSC_VER

namespace idra {

// Forward declarations ///////////////////////////////////////////////////
static void set_thread_name( std::thread& thread, StringView name );

// Thread /////////////////////////////////////////////////////////////////
Thread::Thread( StringView name ) : name( name ) {

}

Thread::~Thread() {
    join();
}

void Thread::start() {
    if ( !thread.joinable() ) {
        stop_request = std::promise<void>();
        thread = std::thread(
            &Thread::run, this,
            std::move( stop_request.get_future() )
        );
    }

    set_thread_name( thread, name );
}

void Thread::stop() {
    try {
        stop_request.set_value();
    } catch ( std::future_error const& ex ) {
        // ignore exception in case of multiple calls to 'stop' function
    }
}

void Thread::join() {
    if ( thread.joinable() ) {
        thread.join();
    }
}

bool Thread::is_stop_requested( std::future<void> const& token ) noexcept {
    if ( token.valid() ) {
        auto status = token.wait_for( std::chrono::milliseconds{ 0 } );
        if ( std::future_status::timeout != status ) {
            return true;
        }
    }

    return false;
}

// ThreadLambda ///////////////////////////////////////////////////////////
ThreadLambda::ThreadLambda( StringView name ) : thread(), running( false ), name( name ) {

}

ThreadLambda::~ThreadLambda() {
    join();
}

ThreadLambda::ThreadLambda( const ThreadLambda& other ) {

}

void ThreadLambda::start( std::function<void( ThreadLambda& )> threadFunc ) {
    running = true;
    if ( !thread.joinable() ) {
        thread = std::thread( [ this, threadFunc ]() {
            threadFunc( *this ); // Execute the provided lambda function
                              } );
    }

    set_thread_name( thread, name );
}

void ThreadLambda::stop() {
    running = false;
    join();
}

void ThreadLambda::join() {
    if ( thread.joinable() ) {
        thread.join();
    }
}

bool ThreadLambda::is_stop_requested( std::future<void> const& token ) noexcept {
    if ( token.valid() ) {
        auto status = token.wait_for( std::chrono::milliseconds{ 0 } );
        if ( std::future_status::timeout != status ) {
            return true;
        }
    }

    return false;
}

// Utility functions //////////////////////////////////////////////////////
void set_thread_name( std::thread& thread, StringView name ) {

#if defined (_MSC_VER)
    BookmarkAllocator* allocator = g_memory->get_thread_allocator();
    const sizet marker = allocator->get_marker();

    char16_t* string_buffer = ( char16_t* )ialloc( name.size * sizeof( char16_t ), allocator );

    utf8_to_utf16( name.data, string_buffer );
    string_buffer[ name.size ] = 0;

    HRESULT r;
    r = SetThreadDescription( thread.native_handle(), ( wchar_t* )string_buffer );

    allocator->free_marker( marker );
#else
    pthread_setname_np( thread.native_handle(), name.data );
#endif // _MSC_VER
}
	
} // namespace idra
