/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "log.hpp"
#include "array.hpp"

#include <stdarg.h>
#include <stdio.h>

#if defined(_MSC_VER)
#include <Windows.h>
#endif

namespace idra {

static constexpr u32    k_string_buffer_size = 8192;
static LogService       s_log_service;

static Array<PrintCallback> callbacks;

extern LogService*      g_log = &s_log_service;

static void output_console( char* log_buffer_ ) {
    printf( "%s", log_buffer_ );
}

#if defined(_MSC_VER)
static void output_visual_studio( char* log_buffer_ ) {
    OutputDebugStringA( log_buffer_ );
}
#endif

void variadic_print_format( cstring format, va_list arguments ) {

    // Define a thread local buffer to print stuff.
    static thread_local char log_buffer[ k_string_buffer_size ];

    // Calculate the length of the string
    //va_list arguments_copy;
    //va_copy( arguments_copy, arguments );
    //// Add +1 for null termination
    //int argument_length = vsnprintf( NULL, 0, format, arguments_copy ) + 1;
    //va_end( arguments_copy );

#if defined(_MSC_VER)
    int len = vsnprintf( log_buffer, k_string_buffer_size, format, arguments );
#else
    int len = vsnprintf( log_buffer, k_string_buffer_size, format, arguments );
#endif

    log_buffer[ len ] = '\0';

    output_console( log_buffer );
#if defined(_MSC_VER)
    output_visual_studio( log_buffer );
#endif // _MSC_VER

    for ( u32 i = 0; i < callbacks.size; ++i ) {
        PrintCallback callback = callbacks[ i ];
        callback( log_buffer );
    }
}

void LogService::print_format( cstring format, ... ) {
    va_list args;
    va_start( args, format );
    variadic_print_format( format, args );
    va_end( args );
}

void LogService::log( LogLevel level, cstring format, ... ) {

    if ( level >= min_log_level ) {
        va_list args;
        va_start( args, format );
        variadic_print_format( format, args );
        va_end( args );
    }
}

void LogService::log_set_min_level( LogLevel level ) {
    min_log_level = level;
}

void LogService::init( Allocator* allocator ) {

    callbacks.init( allocator, 4 );
}

void LogService::shutdown() {
    callbacks.shutdown();
}

void LogService::add_callback( PrintCallback callback ) {
    callbacks.push( callback );
}

void LogService::remove_callback( PrintCallback callback ) {

    for ( u32 i = 0; i < callbacks.size; ++i ) {
        if ( callbacks[ i ] == callback ) {
            callbacks.delete_swap( i );
        }
    }
}

} // namespace idra