/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "kernel/string.hpp"
#include "kernel/memory.hpp"
#include "kernel/log.hpp"
#include "kernel/assert.hpp"

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>

#include "kernel/array.hpp"
#include "kernel/hash_map.hpp"
#include "kernel/allocator.hpp"

#define ASSERT_ON_OVERFLOW

#if defined (ASSERT_ON_OVERFLOW)
#define iassert_overflow() iassert(false)
#else
#define iassert_overflow()
#endif // ASSERT_ON_OVERFLOW

namespace idra {

//
// StringBuffer /////////////////////////////////////////////////////////////////
void StringBuffer::init( sizet size, Allocator* allocator_ ) {
    if ( data ) {
        allocator_->deallocate( data );
    }

    if ( size < 1 ) {
        ilog_error( "ERROR: Buffer cannot be empty!\n" );
        return;
    }

    allocator = allocator_;

    data = ( char* )ialloc( size + 1, allocator_ );
    iassert( data );

    data[ 0 ] = 0;
    buffer_size = ( u32 )size;
    current_size = 0;
}

void StringBuffer::shutdown() {

    ifree( data, allocator );

    buffer_size = current_size = 0;
}

void StringBuffer::append( const char* string ) {
    append_f( "%s", string );
}

void StringBuffer::append_f( const char* format, ... ) {
    if ( current_size >= buffer_size ) {
        iassert_overflow();
        ilog_error( "Buffer full! Please allocate more size.\n" );
        return;
    }

    // TODO: safer version!
    va_list args;
    va_start( args, format );
#if defined(_MSC_VER)
    int written_chars = vsnprintf_s( &data[ current_size ], buffer_size - current_size, _TRUNCATE, format, args );
#else
    int written_chars = vsnprintf( &data[ current_size ], buffer_size - current_size, format, args );
#endif

    current_size += written_chars > 0 ? written_chars : 0;
    va_end( args );

    if ( written_chars < 0 ) {
        iassert_overflow();
        ilog_error( "New string too big for current buffer! Please allocate more size.\n" );
    }
}

void StringBuffer::append( StringView text ) {
    const sizet max_length = current_size + text.size < buffer_size ? text.size : buffer_size - current_size;
    if ( max_length == 0 || max_length >= buffer_size ) {
        iassert_overflow();
        ilog_error( "Buffer full! Please allocate more size.\n" );
        return;
    }

    memcpy( &data[ current_size ], text.data, max_length );
    current_size += ( u32 )max_length;

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[ current_size ] = 0;
}

void StringBuffer::append_m( void* memory, sizet size ) {

    if ( current_size + size >= buffer_size ) {
        iassert_overflow();
        ilog_error( "Buffer full! Please allocate more size.\n" );
        return;
    }

    memcpy( &data[ current_size ], memory, size );
    current_size += ( u32 )size;
}

void StringBuffer::append( const StringBuffer& other_buffer ) {

    if ( other_buffer.current_size == 0 ) {
        return;
    }

    if ( current_size + other_buffer.current_size >= buffer_size ) {
        iassert_overflow();
        ilog_error( "Buffer full! Please allocate more size.\n" );
        return;
    }

    memcpy( &data[ current_size ], other_buffer.data, other_buffer.current_size );
    current_size += other_buffer.current_size;
}


StringView StringBuffer::append_use( cstring string ) {
    return append_use_f( "%s", string );
}

StringView StringBuffer::append_use_f( const char* format, ... ) {
    u32 cached_offset = this->current_size;

    StringView string_span { nullptr, 0 };
    // TODO: safer version!
    // TODO: do not copy paste!
    if ( current_size >= buffer_size ) {
        iassert_overflow();
        ilog_error( "Buffer full! Please allocate more size.\n" );
        return string_span;
    }

    va_list args;
    va_start( args, format );
#if defined(_MSC_VER)
    int written_chars = vsnprintf_s( &data[ current_size ], buffer_size - current_size, _TRUNCATE, format, args );
#else
    int written_chars = vsnprintf( &data[ current_size ], buffer_size - current_size, format, args );
#endif
    current_size += written_chars > 0 ? written_chars : 0;
    va_end( args );

    if ( written_chars < 0 ) {
        ilog_warn( "New string too big for current buffer! Please allocate more size.\n" );
    }

    // Add null termination for string.
    // By allocating one extra character for the null termination this is always safe to do.
    data[ current_size ] = 0;
    ++current_size;

    //return this->data + cached_offset;
    string_span = { this->data + cached_offset, current_size - cached_offset - 1 };
    return string_span;
}

StringView StringBuffer::append_use( StringView text ) {
    u32 cached_offset = this->current_size;

    append( text );
    ++current_size;

    //return this->data + cached_offset;
    StringView string_span{ this->data + cached_offset, current_size - cached_offset - 1 };
    return string_span;
}

StringView StringBuffer::append_use_substring( const char* string, u32 start_index, u32 end_index ) {
    u32 size = end_index - start_index;
    if ( current_size + size >= buffer_size ) {
        iassert_overflow();
        ilog_error( "Buffer full! Please allocate more size.\n" );
        return {nullptr, 0};
    }   

    u32 cached_offset = this->current_size;

    memcpy( &data[ current_size ], string, size );
    current_size += size;

    data[ current_size ] = 0;
    ++current_size;

    //return this->data + cached_offset;
    StringView string_span{ this->data + cached_offset, current_size - 1 };
    return string_span;
}

void StringBuffer::close_current_string() {
    data[ current_size ] = 0;
    ++current_size;
}
//
//u32 StringBuffer::get_index( cstring text ) const {
//    u64 text_distance = text - data;
//    // TODO: how to handle an error here ?
//    return text_distance < buffer_size ? u32( text_distance ) : u32_max;
//}
//
//cstring StringBuffer::get_text( u32 index ) const {
//    // TODO: how to handle an error here ?
//    return index < buffer_size ? cstring(data + index) : nullptr;
//}

char* StringBuffer::reserve( sizet size ) {
    if ( current_size + size >= buffer_size )
        return nullptr;

    u32 offset = current_size;
    current_size += ( u32 )size;

    return data + offset;
}

void StringBuffer::clear() {
    current_size = 0;
    data[ 0 ] = 0;
}

// StringArray ////////////////////////////////////////////////////////////
void StringArray::init( u32 size, Allocator* allocator_ ) {

    allocator = allocator_;
    // Allocate also memory for the hash map
    char* allocated_memory = ( char* )allocator_->allocate( size + sizeof(FlatHashMap<u64, u32>) + sizeof(Array<u32>), 1 );
    string_to_index = ( FlatHashMap<u64, u32>* )allocated_memory;
    string_to_index->init( allocator, 8 );
    string_to_index->set_default_value( u32_max );

    string_indices = ( Array<u32>* )allocated_memory + sizeof( FlatHashMap<u64, u32> );
    string_indices->init( allocator, 8 );

    data = allocated_memory + sizeof( FlatHashMap<u64, u32> ) + sizeof( Array<u32> );

    buffer_size = size;
    current_size = 0;
}

void StringArray::shutdown() {
    // string_to_index contains ALL the memory including data.
    string_to_index->shutdown();
    string_indices->shutdown();

    ifree( string_to_index, allocator );

    buffer_size = current_size = 0;
}

void StringArray::clear() {
    current_size = 0;
    
    string_to_index->clear();
    string_indices->clear();
}

cstring StringArray::intern( cstring string ) {
    static sizet seed = 0xf2ea4ffad;
    const sizet length = strlen( string );
    const sizet hashed_string = idra::hash_bytes( ( void* )string, length, seed );

    u32 string_index = string_to_index->get( hashed_string );
    if ( string_index != u32_max ) {
        string_indices->push( string_index );
        return data + string_index;
    }

    string_index = current_size;
    // Increase current buffer with new interned string
    current_size += ( u32 )length + 1; // null termination
    strcpy( data + string_index, string );

    // Update hash map
    string_to_index->insert( hashed_string, string_index );

    string_indices->push( string_index );

    return data + string_index;
}

sizet StringArray::get_string_count() const {
    return string_to_index->size;
}


cstring StringArray::get_string( u32 index ) const {
    if ( index < string_indices->size ) {
        u32 data_index = string_indices->data[ index ];
        return data + data_index;
    }
    return nullptr;
}

} // namespace idra