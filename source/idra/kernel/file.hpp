/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string_view.hpp"

#include <stdio.h>

namespace idra {

    struct Allocator;
    struct StringArray;

    using FileHandle                = FILE*;
    using FileTime                  = u64;

    static const u32                k_max_path = 512;

    //
    //
    struct Directory {
        char                        path[ k_max_path ];

        void*                       os_handle;
    }; // struct Directory

    // File Input-Output //////////////////////////////////////////////////
    FileHandle                      file_open_for_read( StringView path );
    FileHandle                      file_open_for_write( StringView path );
    void                            file_close( FileHandle file );

    // File read/write using preallocated buffers.
    // User is responsible to allocate and free the memory.
    template<typename T>
    sizet file_read( FileHandle file, T* buffer, sizet size ) {
        #if defined(_WIN64)
            // Correct: use elementcount as filesize, bytes_read becomes the actual bytes read
            // AFTER the end of line conversion for Windows (it uses \r\n).
            const sizet bytes_read = fread_s( buffer, size, 1, size, file );
            return bytes_read;
        #else
            const sizet bytes_read = fread( buffer, 1, size, file );
            return bytes_read;
        #endif // _WIN64
    }
    sizet                           file_read( FileHandle file, Span<char>& buffer, sizet size );
    sizet                           file_read_offset( FileHandle file, Span<char>& buffer, sizet size, sizet offset );
    sizet                           file_write( FileHandle file, Span<const char> buffer );

    // Open file, allocates memory and fill the provided span.
    Span<char>                      file_read_allocate( StringView path, Allocator* allocator );

    // File-System interaction ////////////////////////////////////////////
    // TODO: convert everything to use a path?
    bool                            fs_file_exists( StringView path );
    bool                            fs_file_delete( StringView path );
    bool                            fs_file_copy( StringView existing_file, StringView new_file );

    long                            fs_file_get_size( FileHandle file );
    sizet                           fs_file_get_size( StringView path );

    FileTime                        fs_file_last_write_time( StringView filename );

    bool                            fs_directory_exists( StringView path );
    bool                            fs_directory_create( StringView path );
    bool                            fs_directory_delete( StringView path );

    void                            fs_directory_current( Directory* directory );
    void                            fs_directory_change( StringView path );

    void                            fs_open_directory( cstring path, Directory* out_directory );
    void                            fs_close_directory( Directory* directory );
    void                            fs_parent_directory( Directory* directory );
    void                            fs_sub_directory( Directory* directory, cstring sub_directory_name );

    void                            fs_find_files_in_path( cstring file_pattern, StringArray& files );            // Search files matching file_pattern and puts them in files array.
                                                                                                                    // Examples: "..\\data\\*", "*.bin", "*.*"
    void                            fs_find_files_in_path( cstring extension, cstring search_pattern,
                                                           StringArray& files, StringArray& directories );        // Search files and directories using search_patterns.

    // Path methods ///////////////////////////////////////////////////////
    // Try to resolve path to non-relative version.
    u32                             fs_file_resolve_to_full_path( cstring path, char* out_full_path, u32 max_size );

    // TODO: create a path struct
    // In-place path methods
    void                            file_directory_from_path( char* path ); // Retrieve path without the filename. Path is a preallocated string buffer. It moves the terminator before the name of the file.
    void                            file_name_from_path( char* path );
    StringView                      file_extension_from_path( StringView path );


    // OS methods /////////////////////////////////////////////////////////
    // TODO: move
    void                            environment_variable_get( cstring name, char* output, u32 output_size );

} // namespace idra
