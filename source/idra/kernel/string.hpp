/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string_view.hpp"

namespace idra {

    // Forward declarations ///////////////////////////////////////////////
    struct Allocator;

    template <typename K, typename V>
    struct FlatHashMap;

    struct FlatHashMapIterator;
    
    template <typename T>
    struct Array;

    //
    // Class that preallocates a buffer and appends strings to it. Reserve an additional byte for the null termination when needed.
    struct StringBuffer {

        void                        init( sizet size, Allocator* allocator );
        void                        shutdown();

        // Append a string until it is ready to be used.
        void                        append( const char* string );
        void                        append( StringView text );
        void                        append_m( void* memory, sizet size );       // Memory version of append.
        void                        append( const StringBuffer& other_buffer );
        void                        append_f( const char* format, ... );        // Formatted version of append.
        void                        close_current_string();

        // 
        // Append and returns a pointer to the start of the null-terminated string.
        StringView                  append_use( cstring string );
        StringView                  append_use_f( const char* format, ... );
        StringView                  append_use( StringView text );

        // Append a substring of the passed string.
        StringView                  append_use_substring( const char* string, u32 start_index, u32 end_index );

        // TODO: remove?
        // Index interface
        /*u32                         get_index( cstring text ) const;
        cstring                     get_text( u32 index ) const;*/

        char*                       reserve( sizet size );

        char*                       current()       { return data + current_size; }

        void                        clear();

        char*                       data            = nullptr;
        u32                         buffer_size     = 1024;
        u32                         current_size    = 0;
        Allocator*                  allocator       = nullptr;

    }; // struct StringBuffer

    //
    //
    struct StringArray {

        void                        init( u32 size, Allocator* allocator );
        void                        shutdown();
        void                        clear();

        // Save the passed string.
        cstring                     intern( cstring string );

        sizet                       get_string_count() const;
        cstring                     get_string( u32 index ) const;        

        Array<u32>*                 string_indices;
        FlatHashMap<u64, u32>*      string_to_index;    // Note: trying to avoid bringing the hash map header.

        char*                       data                    = nullptr;
        u32                         buffer_size             = 1024;
        u32                         current_size            = 0;
        
        Allocator*                  allocator               = nullptr;

    }; // struct StringArray


} // namespace idra
