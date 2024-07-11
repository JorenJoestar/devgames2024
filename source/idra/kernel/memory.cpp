/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "kernel/memory.hpp"
#include "kernel/allocator.hpp"
#include "kernel/assert.hpp"
#include "kernel/color.hpp"
#include "kernel/numerics.hpp"

#include <stdlib.h>
#include <memory.h>
#include <cmath>
#include <stdio.h>

#if defined IDRA_IMGUI
#include "external/imgui/imgui.h"
#endif // IDRA_IMGUI

namespace idra {

//#define IDRA_MEMORY_DEBUG
#if defined (IDRA_MEMORY_DEBUG)
    #define imem_assert(cond) iassert(cond)
#else
    #define imem_assert(cond)
#endif // IDRA_MEMORY_DEBUG

// Memory Service /////////////////////////////////////////////////////////

static MemoryService s_memory_service;
extern MemoryService* g_memory = &s_memory_service;

// StackAllocator
struct StackAllocator : public BookmarkAllocator {

    StackAllocator( void* preallocated_memory, sizet size );
};

StackAllocator::StackAllocator( void* preallocated_memory, sizet size ) {

    this->memory = (u8*)preallocated_memory;
    this->allocated_size = 0;
    this->parent_allocator = nullptr;
    this->total_size = size;
}

//
// MemoryService //////////////////////////////////////////////////////////

// Root allocator
static TLSFAllocator                system_allocator;
static LinearAllocator              resident_allocator;
static Allocator*                   current_allocator = nullptr;

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )

//
//
struct AllocatorTrackerNodes {

    Allocator*                      allocator;
    cstring                         name;
    
}; // struct AllocatorTrackerNodes

static constexpr u32                k_max_allocators_tracked = 32;

//
//
struct AllocatorTrackerTree {

    void                            add( Allocator* allocator, Allocator* parent, cstring name );
    void                            remove( Allocator* allocator );

    void                            debug_ui();

    AllocatorTrackerNodes           nodes[ k_max_allocators_tracked ];
    u8                              parent[ k_max_allocators_tracked ];
    u8                              depth[ k_max_allocators_tracked ];
    
    u8                              num_allocators = 0;
}; // struct AllocatorTrackerTree


void AllocatorTrackerTree::add( Allocator* allocator, Allocator* parent_, cstring name ) {
    // Search parent.
    // For small number of trackers, no hash map is needed.
    // Parent index is 0 if parent is null.
    u32 parent_index = 0;

    if ( parent_ ) {
        for ( u32 i = 0; i < num_allocators; ++i ) {
            if ( nodes[ i ].allocator == parent_ ) {
                parent_index = i;
                break;
            }
        }
    }
    
    // Found parent!
    if ( parent_index < num_allocators ) {
        nodes[ num_allocators ].allocator = allocator;
        nodes[ num_allocators ].name = name;
        parent[ num_allocators ] = (u8)parent_index;
        depth[ num_allocators ] = depth[ parent_index ] + 1;
        ++num_allocators;
    }
    else {
        ilog_error( "Error finding allocator, index %u\n", parent_index );
    }
}

void AllocatorTrackerTree::remove( Allocator* allocator ) {

    for ( u32 i = 0; i < num_allocators; ++i ) {
        if ( nodes[ i ].allocator == allocator ) {

            // TODO: improve
            nodes[ i ].allocator = nullptr;

            --num_allocators;
            // Swap
            nodes[ i ] = nodes[ num_allocators ];
            parent[ i ] = parent[ num_allocators ];
            depth[ i ] = depth[ num_allocators ];
            
            break;
        }
    }
}

enum MemoryUnits {
    bytes,
    kilobytes,
    megabytes,
    gigabytes
};

void AllocatorTrackerTree::debug_ui() {

#if defined ( IDRA_IMGUI )
    ImGui::Text( "Allocators tree" );

    static const char* items[] = { "Bytes", "Kilobytes", "Megabytes", "Gigabytes" };
    static const char* items_names[] = { " b", "kb", "mb", "gb" };

    static i32 units = kilobytes;
    ImGui::Combo( "Units", &units, items, IM_ARRAYSIZE( items ) );

    f32 units_divider = 1.f;
    switch ( units ) {
        case kilobytes:
            units_divider = 1 / 1024.f;
            break;

        case megabytes:
            units_divider = 1 / ( 1024.f * 1024.f );
            break;

        case gigabytes:
            units_divider = 1 / ( 1024.f * 1024.f * 1024.f );
            break;
    }

    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        f32 widget_height = canvas_size.y / 3; // 3 = max tree depth + 1

        f32 legend_width = 250;
        f32 graph_width = fabsf( canvas_size.x - legend_width );
        
        f64 new_average = 0;
        const f32 max_widget_size_rcp = 1.f / system_allocator.total_size * canvas_size.x;

        static char buf[ 128 ];

        ImGuiIO& io = ImGui::GetIO();

        f32 current_pos_x = cursor_pos.x;
        f32 current_pos_y = cursor_pos.y;

        const ImVec2 mouse_pos = io.MousePos;

        // Draw rects of depth = 1
        for ( u32 i = 0; i < num_allocators; ++i ) {
            if ( depth[ i ] == 0 && nodes[ i ].allocator ) {
                MemoryStatistics stats = nodes[ i ].allocator->get_statistics();

                const f32 allocated_size = stats.allocated_bytes * max_widget_size_rcp;
                const f32 free_size = ( stats.total_bytes - stats.allocated_bytes ) * max_widget_size_rcp;
                const f32 total_size = stats.total_bytes * max_widget_size_rcp;

                const f32 min_x = current_pos_x;
                const f32 min_y = current_pos_y;
                const f32 max_x = min_x + total_size;
                const f32 max_y = min_y + widget_height;

                draw_list->AddRectFilled( { current_pos_x, current_pos_y }, { current_pos_x + allocated_size, current_pos_y + widget_height }, Color::red().abgr );
                current_pos_x += allocated_size;

                draw_list->AddRectFilled( { current_pos_x, current_pos_y }, { current_pos_x + free_size, current_pos_y + widget_height }, Color::green().abgr );
                current_pos_x += free_size;

                sprintf( buf, "%s", nodes[ i ].name );
                draw_list->AddText( { min_x + 2, min_y + 2 }, Color::white().abgr, buf );

                sprintf( buf, "alloc %.2f%s, free %.2f%s", stats.allocated_bytes * units_divider, items_names[ units ],
                         ( stats.total_bytes - stats.allocated_bytes ) * units_divider, items_names[ units ] );
                draw_list->AddText( { min_x + 2, min_y + 2 + ImGui::GetTextLineHeight() }, Color::white().abgr, buf );

                if ( mouse_pos.x >= min_x && mouse_pos.x <= max_x && mouse_pos.y >= min_y && mouse_pos.y <= max_y ) {

                    ImGui::SetTooltip( "allocated %.2f%s, free %.2f%s", stats.allocated_bytes * units_divider, items_names[ units ],
                                       ( stats.total_bytes - stats.allocated_bytes ) * units_divider, items_names[ units ] );
                }
            }
        }

        current_pos_x = cursor_pos.x;
        current_pos_y += widget_height;


        // Draw rects of depth = 1
        for ( u32 i = 0; i < num_allocators; ++i ) {
            if ( depth[ i ] == 1 && nodes[ i ].allocator ) {
                MemoryStatistics stats = nodes[ i ].allocator->get_statistics();

                const f32 allocated_size = stats.allocated_bytes * max_widget_size_rcp;
                const f32 free_size = ( stats.total_bytes - stats.allocated_bytes ) * max_widget_size_rcp;
                const f32 total_size = stats.total_bytes * max_widget_size_rcp;

                const f32 min_x = current_pos_x;
                const f32 min_y = current_pos_y;
                const f32 max_x = min_x + total_size;
                const f32 max_y = min_y + widget_height;

                draw_list->AddRectFilled( { current_pos_x, current_pos_y }, { current_pos_x + allocated_size, current_pos_y + widget_height }, Color::red().abgr );
                current_pos_x += allocated_size;
                
                draw_list->AddRectFilled( { current_pos_x, current_pos_y }, { current_pos_x + free_size, current_pos_y + widget_height }, Color::green().abgr );
                current_pos_x += free_size;

                sprintf( buf, "%s", nodes[ i ].name );
                draw_list->AddText( { min_x + 2, min_y + 2 }, Color::white().abgr, buf );

                sprintf( buf, "alloc %.2f%s, free %.2f%s", stats.allocated_bytes * units_divider, items_names[ units ], 
                         ( stats.total_bytes - stats.allocated_bytes ) * units_divider, items_names[ units ] );
                draw_list->AddText( { min_x + 2, min_y + 2 + ImGui::GetTextLineHeight() }, Color::white().abgr, buf );

                if ( mouse_pos.x >= min_x && mouse_pos.x <= max_x && mouse_pos.y >= min_y && mouse_pos.y <= max_y ) {

                    ImGui::SetTooltip( "allocated %.2f%s, free %.2f%s", stats.allocated_bytes * units_divider, items_names[ units ],
                                       ( stats.total_bytes - stats.allocated_bytes ) * units_divider, items_names[ units ] );
                }
            }
        }

        current_pos_x = cursor_pos.x;
        current_pos_y += widget_height;
        // Draw rects of depth = 2
        for ( u32 i = 0; i < num_allocators; ++i ) {
            if ( depth[ i ] == 2 && nodes[ i ].allocator ) {
                MemoryStatistics stats = nodes[ i ].allocator->get_statistics();

                f32 allocated_size = stats.allocated_bytes * max_widget_size_rcp;
                f32 free_size = ( stats.total_bytes - stats.allocated_bytes ) * max_widget_size_rcp;

                draw_list->AddRectFilled( { current_pos_x, current_pos_y }, { current_pos_x + allocated_size, current_pos_y + widget_height }, Color::red().abgr );
                current_pos_x += allocated_size;

                draw_list->AddRectFilled( { current_pos_x, current_pos_y }, { current_pos_x + free_size, current_pos_y + widget_height }, Color::green().abgr );
                current_pos_x += free_size;
            }
        }

        ImGui::Dummy( canvas_size );
    }

    for ( u32 i = 0; i < num_allocators; ++i ) {
        if ( nodes[ i ].allocator ) {
            MemoryStatistics stats = nodes[ i ].allocator->get_statistics();
            cstring parent_name = parent[ i ] < num_allocators ? nodes[ parent[ i ] ].name : "None";
            ImGui::Text( "%s, depth %u, parent %s, allocated %.2f%s, free %.2f%s", 
                         nodes[ i ].name, depth[ i ], parent_name, 
                         stats.allocated_bytes * units_divider, items_names[ units ], 
                         ( stats.total_bytes - stats.allocated_bytes ) * units_divider, items_names[ units ] );
        }
    }

#endif // IDRA_IMGUI
}

// Allocator tracker tree
struct AllocatorTrackerTree         s_allocator_tracker_tree;

#endif // IDRA_MEMORY_TRACK_ALLOCATORS

void MemoryService::init( sizet total_application_size, sizet resident_allocator_size ) {

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    // Init tracker tree
    for ( u32 i = 0; i < k_max_allocators_tracked; ++i ) {
        s_allocator_tracker_tree.depth[ i ] = 0;
        s_allocator_tracker_tree.parent[ i ] = 0;

        s_allocator_tracker_tree.nodes[ i ].allocator = nullptr;
        s_allocator_tracker_tree.nodes[ i ].name = nullptr;
    }

    // Set system allocator as root
    s_allocator_tracker_tree.depth[ 0 ] = 0;
    s_allocator_tracker_tree.parent[ 0 ] = -1;
    s_allocator_tracker_tree.nodes[ 0 ].allocator = &system_allocator;
    s_allocator_tracker_tree.nodes[ 0 ].name = "TLSF Root";
    s_allocator_tracker_tree.num_allocators = 1;
    
#endif // IDRA_MEMORY_TRACK_ALLOCATORS

    ilog( "Memory Service Init\nTotal allocated size %fKb; resident allocator size %fKb\n", 
          total_application_size / 1024.f, resident_allocator_size / 1024.f );

    system_allocator.init( total_application_size );
    resident_allocator.init( &system_allocator, resident_allocator_size, "Resident" );
}

void MemoryService::shutdown() {

    resident_allocator.shutdown();
    system_allocator.shutdown();

    ilog( "Memory Service Shutdown\n" );
}

void* MemoryService::global_malloc( sizet size, sizet alignment ) {
    //ilog( "global malloc of size %llu\n", size );
    iassert( current_allocator );
    //return malloc( size );
    return current_allocator->allocate( size, alignment );
}

void MemoryService::global_free( void* pointer ) {
    //ilog( "global free of %p\n", pointer );
    iassert( current_allocator );
    //free( pointer );
    current_allocator->deallocate( pointer );
}

void* MemoryService::global_realloc( void* pointer, sizet new_size ) {
   // ilog( "global realloc of size %llu of %p\n", new_size, pointer );
    iassert( current_allocator );
    // TODO:
    //return realloc( pointer, new_size );
    current_allocator->deallocate( pointer );
    return current_allocator->allocate( new_size, 1 );
}

Allocator* MemoryService::get_current_allocator() {
    return current_allocator;
}

void MemoryService::set_current_allocator( Allocator* allocator ) {
    current_allocator = allocator;
}

BookmarkAllocator* MemoryService::get_thread_allocator() {
    static thread_local u8 stack_memory[ k_thread_stack_size ];
    static thread_local StackAllocator allocator( &stack_memory, k_thread_stack_size );
    return &allocator;
}

TLSFAllocator* MemoryService::get_system_allocator() {
    return &system_allocator;
}

LinearAllocator* MemoryService::get_resident_allocator() {
    return &resident_allocator;
}

#if defined ( IDRA_IMGUI )

void MemoryService::imgui_draw() {

    if ( ImGui::Begin( "Memory Service" ) ) {

        system_allocator.debug_ui();

        ImGui::Separator();

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )

        s_allocator_tracker_tree.debug_ui();

#endif // IDRA_MEMORY_TRACK_ALLOCATORS

    }
    ImGui::End();
}
#endif // IDRA_IMGUI

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )

void MemoryService::track_allocator( Allocator* allocator, Allocator* parent_allocator, cstring name ) {
    s_allocator_tracker_tree.add( allocator, parent_allocator, name );
}

void MemoryService::untrack_allocator( Allocator* allocator ) {
    s_allocator_tracker_tree.remove( allocator );
}

#endif // IDRA_MEMORY_TRACK_ALLOCATORS

//
//void MemoryService::test() {
//
//    //static u8 mem[ 1024 ];
//    //LinearAllocator la;
//    //la.init( mem, 1024 );
//
//    //// Allocate 3 times
//    //void* a1 = ralloca( 16, &la );
//    //void* a2 = ralloca( 20, &la );
//    //void* a4 = ralloca( 10, &la );
//    //// Free based on size
//    //la.free( 10 );
//    //void* a3 = ralloca( 10, &la );
//    //iassert( a3 == a4 );
//
//    //// Free based on pointer
//    //rfree( a2, &la );
//    //void* a32 = ralloca( 10, &la );
//    //iassert( a32 == a2 );
//    //// Test out of bounds 
//    //u8* out_bounds = ( u8* )a1 + 10000;
//    //rfree( out_bounds, &la );
//}

} // namespace idra