/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "platform.hpp"

namespace idra {

struct Allocator;

//
//
enum LogLevel {

    LogLevel_Debug = 0,
    LogLevel_Info,
    LogLevel_Warning,
    LogLevel_Error

}; // enum LogLevel

// Additional callback for printing
typedef void                    ( *PrintCallback )( const char* );

// Service ////////////////////////////////////////////////////////////////

struct LogService {

    void                        init( Allocator* allocator );
    void                        shutdown();

    void                        log( LogLevel level, cstring format, ... );
    void                        print_format( cstring format, ... );

    void                        log_set_min_level( LogLevel level );

    // Callback 
    void                        add_callback( PrintCallback callback );
    void                        remove_callback( PrintCallback callback );

    LogLevel                    min_log_level   = LogLevel_Debug;

}; // struct LogService

extern LogService*              g_log;


// Helper macros //////////////////////////////////////////////////////////

#if defined(_MSC_VER)
    //#define iprint(format, ...)         idra::print_format( format, __VA_ARGS__ );
    #define ilog_debug( format, ... )   idra::g_log->log( idra::LogLevel_Debug, format, __VA_ARGS__ );
    #define ilog( format, ... )         idra::g_log->log( idra::LogLevel_Info, format, __VA_ARGS__ );
    #define ilog_warn( format, ... )    idra::g_log->log( idra::LogLevel_Warning, format, __VA_ARGS__ );
    #define ilog_error( format, ... )   idra::g_log->log( idra::LogLevel_Error, format, __VA_ARGS__ );
#else
    //#define iprint(format, ...)         idra::print_format(format, ## __VA_ARGS__);
    #define ilog_debug( format, ... )   idra::g_log->log( idra::LogLevel_Debug, format, ## __VA_ARGS__ );
    #define ilog( format, ... )         idra::g_log->log( idra::LogLevel_Debug, format, ## __VA_ARGS__ );
    #define ilog_warn( format, ... )    idra::g_log->log( idra::LogLevel_Debug, format, ## __VA_ARGS__ );
#define ilog_error( format, ... )   idra::g_log->log( idra::LogLevel_Debug, format, ## __VA_ARGS__ );
#endif

} // namespace idra