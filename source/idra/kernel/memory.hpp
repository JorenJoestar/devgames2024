/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/platform.hpp"

// Define to track allocators across the engine
#define IDRA_MEMORY_TRACK_ALLOCATORS

namespace idra {

    struct Allocator;
    struct BookmarkAllocator;
    struct LinearAllocator;
    struct TLSFAllocator;

    // This should be either used to create threads stack size as well.
    static const sizet              k_thread_stack_size = ikilo(64);

    // Memory Methods /////////////////////////////////////////////////////
    void                            mem_copy( void* destination, void* source, sizet size );

    //
    //  Calculate aligned memory size.
    sizet                           mem_align( sizet size, sizet alignment );

    // Memory Service /////////////////////////////////////////////////////
    
    //
    // Preallocate memory at startup and manages other allocators.
    struct MemoryService {

        void                        init( sizet total_application_size, sizet resident_allocator_size );
        void                        shutdown();

        // Methods used by memory hooks to track allocations
        void*                       global_malloc( sizet size, sizet alignment );
        void                        global_free( void* pointer );
        void*                       global_realloc( void* pointer, sizet new_size );

        Allocator*                  get_current_allocator();
        void                        set_current_allocator( Allocator* allocator );

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
        void                        track_allocator( Allocator* allocator, Allocator* parent_allocator, cstring name );
        void                        untrack_allocator( Allocator* allocator );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS

#if defined IDRA_IMGUI
        void                        imgui_draw();
#endif // IDRA_IMGUI

        // Returns a small per thread allocator.
        BookmarkAllocator*          get_thread_allocator();

        // The only allocator actually allocating memory from the OS
        TLSFAllocator*              get_system_allocator();

        // Allocator of everything that will be always present in the application
        LinearAllocator*            get_resident_allocator();

    }; // struct MemoryService

    extern MemoryService*           g_memory;


    // Macro helpers //////////////////////////////////////////////////////
    #define ialloc(size, allocator)    ((allocator)->allocate( size, 1, __FILE__, __LINE__ ))
    #define iallocm(size, allocator)   ((u8*)(allocator)->allocate( size, 1, __FILE__, __LINE__ ))
    #define ialloct(type, allocator)   ((type*)(allocator)->allocate( sizeof(type), 1, __FILE__, __LINE__ ))
    #define ialloca(size, allocator, alignment)    ((allocator)->allocate( size, alignment, __FILE__, __LINE__ ))

    #define ifree(pointer, allocator) (allocator)->deallocate(pointer)
    
} // namespace idra

// Further methods to override
// https://stackoverflow.com/questions/72447710/override-malloc-and-new-calls-in-cpp-program