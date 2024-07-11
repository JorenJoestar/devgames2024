/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "kernel/allocator.hpp"
#include "kernel/memory.hpp"
#include "kernel/assert.hpp"

#include "external/tlsf.h"

#include <stdlib.h>
#include <memory.h>

#if defined IDRA_IMGUI
#include "external/imgui/imgui.h"
#endif // IDRA_IMGUI

//#define IDRA_MEMORY_DEBUG
#if defined (IDRA_MEMORY_DEBUG)
#define imem_assert(cond) iassert(cond)
#else
#define imem_assert(cond)
#endif // IDRA_MEMORY_DEBUG

// Define this and add StackWalker to heavy memory profile
//#define IDRA_MEMORY_STACK

//
#define HEAP_ALLOCATOR_STATS

#if defined (IDRA_MEMORY_STACK)
#include "external/StackWalker.h"
#endif // IDRA_MEMORY_STACK


namespace idra {


//
// Walker methods
static void exit_walker( void* ptr, size_t size, int used, void* user );
static void imgui_walker( void* ptr, size_t size, int used, void* user );

// Memory Structs /////////////////////////////////////////////////////////

// TLSFAllocator //////////////////////////////////////////////////////////
TLSFAllocator::~TLSFAllocator() {
}

void TLSFAllocator::init( sizet size ) {

    size += tlsf_size() + 8;

    // Allocate
    memory = malloc( size );
    total_size = size;
    allocated_size = 0;

    tlsf_handle = tlsf_create_with_pool( memory, size );

    ilog( "TLSFAllocator of size %llu created\n", size );
}

void TLSFAllocator::shutdown() {

    // Check memory at the application exit.
    MemoryStatistics stats{ 0, total_size };
    pool_t pool = tlsf_get_pool( tlsf_handle );
    tlsf_walk_pool( pool, exit_walker, ( void* )&stats );

    if ( stats.allocated_bytes ) {
        ilog( "TLSFAllocator Shutdown.\n===============\nFAILURE! Allocated memory detected. allocated %llu, total %llu\n===============\n\n", stats.allocated_bytes, stats.total_bytes );
    } else {
        ilog( "TLSFAllocator Shutdown - all memory free!\n" );
    }

    iassertm( stats.allocated_bytes == 0, "Allocations still present. Check your code!" );

    tlsf_destroy( tlsf_handle );

    free( memory );
}

#if defined IDRA_IMGUI
void TLSFAllocator::debug_ui() {

    ImGui::Separator();
    ImGui::Text( "TLSF Allocator" );
    ImGui::Separator();
    MemoryStatistics stats{ 0, total_size };
    pool_t pool = tlsf_get_pool( tlsf_handle );
    tlsf_walk_pool( pool, imgui_walker, ( void* )&stats );

    ImGui::Separator();
    ImGui::Text( "\tAllocation count %d", stats.allocation_count );
    ImGui::Text( "\tAllocated %llu K, free %llu Mb, total %llu Mb", stats.allocated_bytes / ( 1024 * 1024 ), ( total_size - stats.allocated_bytes ) / ( 1024 * 1024 ), total_size / ( 1024 * 1024 ) );
}
#endif // IDRA_IMGUI


#if defined (IDRA_MEMORY_STACK)
class IdraStackWalker : public StackWalker {
public:
    IdraStackWalker() : StackWalker() {}
protected:
    virtual void OnOutput( LPCSTR szText ) {
        iprint( "\nStack: \n%s\n", szText );
        StackWalker::OnOutput( szText );
    }
}; // class IdraStackWalker

void* TLSFAllocator::allocate( sizet size, sizet alignment ) {

    /*if ( size == 16 )
    {
        IdraStackWalker sw;
        sw.ShowCallstack();
    }*/

    void* mem = tlsf_malloc( tlsf_handle, size );
    iprint( "Mem: %p, size %llu \n", mem, size );
    return mem;
}
#else

void* TLSFAllocator::allocate( sizet size, sizet alignment ) {
#if defined (HEAP_ALLOCATOR_STATS)
    void* allocated_memory = alignment == 1 ? tlsf_malloc( tlsf_handle, size ) : tlsf_memalign( tlsf_handle, alignment, size );
    sizet actual_size = tlsf_block_size( allocated_memory );
    allocated_size += actual_size;

    /*if ( size == 272 ) {
        return allocated_memory;
    }*/
    return allocated_memory;
#else
    void* allocated_memory = alignment == 1 ? tlsf_malloc( tlsf_handle, size ) : tlsf_memalign( tlsf_handle, alignment, size );
    return allocated_memory;
#endif // HEAP_ALLOCATOR_STATS
}
#endif // IDRA_MEMORY_STACK

void* TLSFAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void TLSFAllocator::deallocate( void* pointer ) {
#if defined (HEAP_ALLOCATOR_STATS)
    sizet actual_size = tlsf_block_size( pointer );
    allocated_size -= actual_size;

    tlsf_free( tlsf_handle, pointer );
#else
    tlsf_free( tlsf_handle, pointer );
#endif
}

MemoryStatistics TLSFAllocator::get_statistics() const {
    return { .allocated_bytes = allocated_size, .total_bytes = total_size, .allocation_count = 1 };
}

// LinearAllocator /////////////////////////////////////////////////////////

LinearAllocator::~LinearAllocator() {
}

void LinearAllocator::init( Allocator* parent_allocator_, sizet size, StringView name ) {

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->track_allocator( this, parent_allocator_, name.data );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS

    parent_allocator = parent_allocator_;
    memory = iallocm( size, parent_allocator_ );
    iassert( memory );

    total_size = size;
    allocated_size = 0;
}

void LinearAllocator::shutdown() {
    clear();
    ifree( memory, parent_allocator );

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->untrack_allocator( this );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void* LinearAllocator::allocate( sizet size, sizet alignment ) {
    iassert( size > 0 );

    const sizet new_start = mem_align( allocated_size, alignment );
    iassert( new_start < total_size );
    const sizet new_allocated_size = new_start + size;
    if ( new_allocated_size > total_size ) {
        imem_assert( false && "Overflow" );
        return nullptr;
    }

    allocated_size = new_allocated_size;
    return memory + new_start;
}

void* LinearAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void LinearAllocator::deallocate( void* ) {
    // This allocator does not allocate on a per-pointer base!
}

void LinearAllocator::clear() {
    allocated_size = 0;
}

MemoryStatistics LinearAllocator::get_statistics() const {
    return { .allocated_bytes = allocated_size, .total_bytes = total_size, .allocation_count = 1 };
}

// Memory Methods /////////////////////////////////////////////////////////
void mem_copy( void* destination, void* source, sizet size ) {
    memcpy( destination, source, size );
}

sizet mem_align( sizet size, sizet alignment ) {
    const sizet alignment_mask = alignment - 1;
    return ( size + alignment_mask ) & ~alignment_mask;
}

// MallocAllocator ///////////////////////////////////////////////////////
void* MallocAllocator::allocate( sizet size, sizet alignment ) {
    return malloc( size );
}

void* MallocAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return malloc( size );
}

void MallocAllocator::deallocate( void* pointer ) {
    free( pointer );
}

// BookmarkAllocator //////////////////////////////////////////////////////
void BookmarkAllocator::init( Allocator* parent_allocator_, sizet size, StringView name ) {

    parent_allocator = parent_allocator_;
    memory = iallocm( size, parent_allocator_ );
    iassert( memory );

    allocated_size = 0;
    total_size = size;

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->track_allocator( this, parent_allocator_, name.data );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void BookmarkAllocator::shutdown() {
    ifree( memory, parent_allocator );

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->untrack_allocator( this );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void* BookmarkAllocator::allocate( sizet size, sizet alignment ) {
    iassert( size > 0 );

    const sizet new_start = mem_align( allocated_size, alignment );
    iassert( new_start < total_size );
    const sizet new_allocated_size = new_start + size;
    if ( new_allocated_size > total_size ) {
        imem_assert( false && "Overflow" );
        return nullptr;
    }

    allocated_size = new_allocated_size;
    return memory + new_start;
}

void* BookmarkAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void BookmarkAllocator::deallocate( void* pointer ) {

    iassert( pointer >= memory );
    iassertm( pointer < memory + total_size, "Out of bound free on linear allocator (outside bounds). Tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", ( u8* )pointer, ( u8* )pointer - memory, memory, total_size, allocated_size );
    iassertm( pointer < memory + allocated_size, "Out of bound free on linear allocator (inside bounds, after allocated). Tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", ( u8* )pointer, ( u8* )pointer - memory, memory, total_size, allocated_size );

    const sizet size_at_pointer = ( u8* )pointer - memory;

    allocated_size = size_at_pointer;
}

MemoryStatistics BookmarkAllocator::get_statistics() const {
    return { .allocated_bytes = allocated_size, .total_bytes = total_size, .allocation_count = 1 };
}

sizet BookmarkAllocator::get_marker() {
    return allocated_size;
}

void BookmarkAllocator::free_marker( sizet marker ) {
    const sizet difference = marker - allocated_size;
    if ( difference > 0 ) {
        allocated_size = marker;
    }
}

void BookmarkAllocator::clear() {
    allocated_size = 0;
}

// DoubleStackAllocator //////////////////////////////////////////////////
void DoubleBookmarkAllocator::init( Allocator* parent_allocator_, sizet size, StringView name ) {

    parent_allocator = parent_allocator_;
    memory = iallocm( size, parent_allocator_ );
    iassert( memory );

    top = size;
    bottom = 0;
    total_size = size;

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->track_allocator( this, parent_allocator_, name.data );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void DoubleBookmarkAllocator::shutdown() {
    ifree( memory, parent_allocator );

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->untrack_allocator( this );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void* DoubleBookmarkAllocator::allocate( sizet size, sizet alignment ) {
    iassert( false );
    return nullptr;
}

void* DoubleBookmarkAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    iassert( false );
    return nullptr;
}

void DoubleBookmarkAllocator::deallocate( void* pointer ) {
    iassert( false );
}

MemoryStatistics DoubleBookmarkAllocator::get_statistics() const {
    return { .allocated_bytes = (total_size - top + bottom), .total_bytes = total_size, .allocation_count = 1 };
}

void* DoubleBookmarkAllocator::allocate_top( sizet size, sizet alignment ) {
    iassert( size > 0 );

    const sizet new_start = mem_align( top - size, alignment );
    if ( new_start <= bottom ) {
        imem_assert( false && "Overflow Crossing" );
        return nullptr;
    }

    top = new_start;
    return memory + new_start;
}

void* DoubleBookmarkAllocator::allocate_bottom( sizet size, sizet alignment ) {
    iassert( size > 0 );

    const sizet new_start = mem_align( bottom, alignment );
    const sizet new_allocated_size = new_start + size;
    if ( new_allocated_size >= top ) {
        imem_assert( false && "Overflow Crossing" );
        return nullptr;
    }

    bottom = new_allocated_size;
    return memory + new_start;
}

void DoubleBookmarkAllocator::deallocate_top( sizet size ) {
    if ( size > total_size - top ) {
        top = total_size;
    } else {
        top += size;
    }
}

void DoubleBookmarkAllocator::deallocate_bottom( sizet size ) {
    if ( size > bottom ) {
        bottom = 0;
    } else {
        bottom -= size;
    }
}

sizet DoubleBookmarkAllocator::get_top_marker() {
    return top;
}

sizet DoubleBookmarkAllocator::get_bottom_marker() {
    return bottom;
}

void DoubleBookmarkAllocator::free_top_marker( sizet marker ) {
    if ( marker > top && marker < total_size ) {
        top = marker;
    }
}

void DoubleBookmarkAllocator::free_bottom_marker( sizet marker ) {
    if ( marker < bottom ) {
        bottom = marker;
    }
}

void DoubleBookmarkAllocator::clear_top() {
    top = total_size;
}

void DoubleBookmarkAllocator::clear_bottom() {
    bottom = 0;
}

// SlotAllocator //////////////////////////////////////////////////////////
SlotAllocator::~SlotAllocator() {
}

void SlotAllocator::init( Allocator* parent_allocator_, sizet slot_count_, sizet element_size_, StringView name ) {

    parent_allocator = parent_allocator_;
    // HY_ASSERT( elementSize >= sizeof( uintptr_t ), "Error: memory slots must be used only on structures with size >= 4." );
    memory = iallocm( slot_count_ * element_size_, parent_allocator );
    iassert( memory );

    used_slots = 0;
    total_slots = ( u32 )slot_count_;
    element_size = element_size_;

    void* currentAddress;

    // Init all the memory to be a free list: each free block contains the pointer to the next free element.
    for ( u32 i = 0; i < slot_count_; ++i ) {
        currentAddress = &( memory[ i * element_size_ ] );
        *( ( uintptr* )currentAddress ) = ( uintptr )( currentAddress )+element_size_;
    }

    next_free_address = memory;

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->track_allocator( this, parent_allocator_, name.data );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void SlotAllocator::shutdown() {

    //DumpAllocations();
    iassert( used_slots == 0 );
    ifree( memory, parent_allocator );

#if defined ( IDRA_MEMORY_TRACK_ALLOCATORS )
    g_memory->untrack_allocator( this );
#endif // IDRA_MEMORY_TRACK_ALLOCATORS
}

void* SlotAllocator::allocate( sizet size, sizet alignment ) {
    iassert( size == element_size );

    if ( used_slots < total_slots ) {
        void* freeMemory = find_next_free_slot();
        ++used_slots;
        return freeMemory;
    }

    return nullptr;
}

void* SlotAllocator::allocate( sizet size, sizet alignment, cstring file, i32 line ) {
    return allocate( size, alignment );
}

void SlotAllocator::deallocate( void* pointer ) {
    if ( used_slots > 0 ) {
        // Mark this as a free
        *( ( uintptr* )pointer ) = ( uintptr )next_free_address;
        next_free_address = pointer;
        --used_slots;
    }
}

MemoryStatistics SlotAllocator::get_statistics() const {
    return { .allocated_bytes = used_slots * element_size, .total_bytes = total_slots * element_size, .allocation_count = 1 };
}

void* SlotAllocator::find_next_free_slot() {
    void* freeMemory = next_free_address;
    // Update next free address: follow the current address written in the next free pointer
    // and update next to be this value.
    void* nextAddress = ( void* )*( ( uintptr* )next_free_address );
    next_free_address = nextAddress;

    return freeMemory;
}

sizet SlotAllocator::get_free_memory() {
    return ( total_slots - used_slots ) * element_size;
}


void exit_walker( void* ptr, size_t size, int used, void* user ) {
    MemoryStatistics* stats = ( MemoryStatistics* )user;
    stats->add( used ? size : 0 );

    if ( used ) {
        ilog_warn( "Found active allocation %p, %llu\n", ptr, size );
    }
}

#if defined IDRA_IMGUI
void imgui_walker( void* ptr, size_t size, int used, void* user ) {

    u32 memory_size = ( u32 )size;
    cstring memory_unit = " b";
    if ( memory_size > 1024 * 1024 ) {
        memory_size /= 1024 * 1024;
        memory_unit = "Mb";
    } else if ( memory_size > 1024 ) {
        memory_size /= 1024;
        memory_unit = "kb";
    }
    ImGui::Text( "\t%p %s size: %4llu %s\n", ptr, used ? "used" : "free", memory_size, memory_unit );

    MemoryStatistics* stats = ( MemoryStatistics* )user;
    stats->add( used ? size : 0 );
}

#endif // IDRA_IMGUI
} // namespace idra