/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string_view.hpp"

namespace idra {


    // Memory Structs /////////////////////////////////////////////////////
    //
    //
    struct MemoryStatistics {

        sizet                       allocated_bytes;
        sizet                       total_bytes;

        u32                         allocation_count;

        void add( sizet a ) {
            if ( a ) {
                allocated_bytes += a;
                ++allocation_count;
            }
        }
    }; // struct MemoryStatistics

    //
    //
    struct Allocator {
        virtual ~Allocator() { }

        virtual void*               allocate( sizet size, sizet alignment ) = 0;
        virtual void*               allocate( sizet size, sizet alignment, cstring file, i32 line ) = 0;

        virtual void                deallocate( void* pointer ) = 0;

        virtual MemoryStatistics    get_statistics() const { return {}; }

        // Helper method
        template <typename T>
        T* allocate( sizet size, sizet alignment, cstring file, i32 line ) {
            return ( T* )allocate( size, alignment, file, line );
        }

    }; // struct Allocator

    //
    // TLSF backed allocator
    struct TLSFAllocator : public Allocator {

        ~TLSFAllocator() override;

        void                        init( sizet size );
        void                        shutdown();

#if defined IDRA_IMGUI
        void                        debug_ui();
#endif // IDRA_IMGUI

        void*                       allocate( sizet size, sizet alignment ) override;
        void*                       allocate( sizet size, sizet alignment, cstring file, i32 line ) override;

        void                        deallocate( void* pointer ) override;

        MemoryStatistics            get_statistics() const override;

        void*                       tlsf_handle;
        void*                       memory;
        sizet                       allocated_size = 0;
        sizet                       total_size = 0;
        
    }; // struct TLSFAllocator

    //
    // Allocator that can be reset to a specific position using a bookmark.
    struct BookmarkAllocator : public Allocator {

        void                        init( Allocator* parent_allocator, sizet size, StringView name );
        
        void                        shutdown();

        void*                       allocate( sizet size, sizet alignment ) override;
        void*                       allocate( sizet size, sizet alignment, cstring file, i32 line ) override;

        void                        deallocate( void* pointer ) override;

        MemoryStatistics            get_statistics() const override;

        sizet                       get_marker();
        void                        free_marker( sizet marker );

        void                        clear();

        u8*                         memory          = nullptr;
        sizet                       total_size      = 0;
        sizet                       allocated_size  = 0;

        Allocator*                  parent_allocator = nullptr;

    }; // struct BookmarkAllocator

    //
    // Allocator that can be reset to specific top/bottom bookmarks
    struct DoubleBookmarkAllocator : public Allocator {

        void                        init( Allocator* parent_allocator, sizet size, StringView name );
        void                        shutdown();

        void*                       allocate( sizet size, sizet alignment ) override;
        void*                       allocate( sizet size, sizet alignment, cstring file, i32 line ) override;
        void                        deallocate( void* pointer ) override;

        MemoryStatistics            get_statistics() const override;

        void*                       allocate_top( sizet size, sizet alignment );
        void*                       allocate_bottom( sizet size, sizet alignment );

        void                        deallocate_top( sizet size );
        void                        deallocate_bottom( sizet size );

        sizet                       get_top_marker();
        sizet                       get_bottom_marker();

        void                        free_top_marker( sizet marker );
        void                        free_bottom_marker( sizet marker );

        void                        clear_top();
        void                        clear_bottom();

        u8*                         memory          = nullptr;
        sizet                       total_size      = 0;
        sizet                       top             = 0;
        sizet                       bottom          = 0;

        Allocator*                  parent_allocator = nullptr;

    }; // struct DoubleBookmarkAllocator

    //
    // Allocator that can only grow and be reset.
    //
    struct LinearAllocator : public Allocator {

        ~LinearAllocator();

        void                        init( Allocator* parent_allocator, sizet size, StringView name );
        void                        shutdown();

        void*                       allocate( sizet size, sizet alignment ) override;
        void*                       allocate( sizet size, sizet alignment, cstring file, i32 line ) override;

        void                        deallocate( void* pointer ) override;

        void                        clear();

        MemoryStatistics            get_statistics() const override;

        u8*                         memory          = nullptr;
        sizet                       total_size      = 0;
        sizet                       allocated_size  = 0;

        Allocator*                  parent_allocator = nullptr;
    }; // struct LinearAllocator

    //
    // Allocator based on fixed size slots, also known as slab allocator.
    // It stores the free list in the non allocated slots memory.
    struct SlotAllocator : public Allocator {

        ~SlotAllocator();

        void                        init( Allocator* parent_allocator, sizet slot_count, sizet element_size, StringView name );
        void                        shutdown();

        void*                       allocate( sizet size, sizet alignment ) override;
        void*                       allocate( sizet size, sizet alignment, cstring file, i32 line ) override;

        void                        deallocate( void* pointer ) override;

        MemoryStatistics            get_statistics() const override;

        void*                       find_next_free_slot();

        sizet                       get_free_memory();

        u8*                         memory              = nullptr;
        void*                       next_free_address   = nullptr;

        sizet                       element_size        = 0;
        sizet                       total_memory        = 0;
        u32                         total_slots         = 0;
        u32                         used_slots          = 0;

        Allocator*                  parent_allocator    = nullptr;        

    }; // struct SlotAllocator

    //
    // DANGER: this should be used for NON runtime processes, like compilation of resources.
    struct MallocAllocator : public Allocator {
        void*                       allocate( sizet size, sizet alignment ) override;
        void*                       allocate( sizet size, sizet alignment, cstring file, i32 line ) override;

        void                        deallocate( void* pointer ) override;
    };


} // namespace idra