/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "file.hpp"

#include "kernel/allocator.hpp"
#include "kernel/memory.hpp"
#include "kernel/assert.hpp"
#include "kernel/string.hpp"

#if defined(_WIN64)
#include <windows.h>
#else
#define MAX_PATH 65536
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sendfile.h>  // sendfile
#include <fcntl.h>         // open
#include <unistd.h>        // close
#endif

#include <string.h>

namespace idra {

FileHandle file_open_for_read( StringView path ) {
#if defined(_WIN64)
    FILE* f;
    fopen_s( &f, path.data, "rb" );
    return f;
#else
    FILE* f = fopen( path.data, "rb ");
    return f;
#endif // _WIN64
}

FileHandle file_open_for_write( StringView path ) {
#if defined(_WIN64)
    FILE* f;
    fopen_s( &f, path.data, "wb" );
    return f;
#else
    FILE* f = fopen( path.data, "wb ");
    return f;
#endif // _WIN64
}

void file_close( FileHandle file ) {
    if ( file ) {
        fclose( file );
    }
}

sizet file_read( FileHandle file, Span<char>& buffer, sizet size ) {

#if defined(_WIN64)
    // Correct: use elementcount as filesize, bytes_read becomes the actual bytes read
    // AFTER the end of line conversion for Windows (it uses \r\n).
    const sizet bytes_read = fread_s( buffer.data, buffer.size, 1, size, file );
    return bytes_read;
#else
    const sizet bytes_read = fread( buffer.data, 1, size, file );
    return bytes_read;
#endif // _WIN64
}

sizet file_read_offset( FileHandle file, Span<char>& buffer, sizet size, sizet offset ) {

#if defined(_WIN64)
    // Move to file offset
    fseek( file, (long)offset, SEEK_SET );
    // Read file
    const sizet bytes_read = fread_s( buffer.data, buffer.size, 1, size, file );
    return bytes_read;
#else
    // Move to file offset
    fseek( file, (long)offset, SEEK_SET );
    const sizet bytes_read = fread( buffer.data, 1, size, file );
    return bytes_read;
#endif // _WIN64
}

sizet file_write( FileHandle file, Span<const char> buffer ) {
    const sizet bytes_written = fwrite( buffer.data, 1, buffer.size, file );
    return bytes_written;
}

Span<char> file_read_allocate( StringView path, Allocator* allocator ) {
    FileHandle file = file_open_for_read( path );
    if ( !file ) {
        ilog_error( "Could not open file %s\n", path.data );
        return Span<char>(0, 0);
    }

    sizet file_size = fs_file_get_size( file );

    char* file_buffer = ( char* )ialloc( file_size + 1, allocator );
    iassert( file_buffer );
    Span<char> file_data( file_buffer, file_size );
    sizet read_bytes = file_read( file, file_data, file_size );
    file_buffer[ read_bytes ] = 0;
    file_close( file );

    return file_data;
}

bool fs_file_copy( StringView existing_file, StringView new_file ) {
#if defined(_WIN64)
    return CopyFileA( existing_file.data, new_file.data, 0 );
#else
    int source = open(existing_file.data, O_RDONLY, 0);
    int dest = open(new_file.data, O_WRONLY | O_CREAT /*| O_TRUNC/**/, 0644);

    // struct required, rationale: function stat() exists also
    struct stat stat_source;
    fstat(source, &stat_source);

    bool successful_copy = sendfile(dest, source, 0, stat_source.st_size) >= 0;

    close(source);
    close(dest);
    return successful_copy;
#endif // _WIN64
}

long fs_file_get_size( FileHandle f ) {
    long fileSizeSigned;

    fseek( f, 0, SEEK_END );
    fileSizeSigned = ftell( f );
    fseek( f, 0, SEEK_SET );

    return fileSizeSigned;
}

sizet fs_file_get_size( StringView path ) {
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA file_info;

    GetFileAttributesEx( path.data, GetFileExInfoStandard, &file_info );

    return ( (sizet)file_info.nFileSizeHigh << 32 ) | file_info.nFileSizeLow;
#else
    struct stat file_details;
    stat( path.data , &file_details );
    return file_details.st_size;
#endif // _WIN64
}

#if defined (_WIN64)
u64 windows_filetime_to_u64( FILETIME filetime ) {
    const u64 packed_filetime = filetime.dwLowDateTime | ( (u64)(filetime.dwHighDateTime) << 32 );
    return packed_filetime;
}
#endif // _WIN64

FileTime fs_file_last_write_time( StringView filename ) {
#if defined(_WIN64)
    u64 last_write_time = 0;

    WIN32_FILE_ATTRIBUTE_DATA data;
    if ( GetFileAttributesExA( filename.data, GetFileExInfoStandard, &data ) ) {
        last_write_time = windows_filetime_to_u64( data.ftLastWriteTime );
    }

    return last_write_time;
#else
    struct stat file_details;
    stat( filename.data , &file_details );
    time_t modify_time = file_details.st_mtime; 
    return modify_time;
#endif // _WIN64
}


u32 fs_file_resolve_to_full_path( cstring path, char* out_full_path, u32 max_size ) {
#if defined(_WIN64)
    return GetFullPathNameA( path, max_size, out_full_path, nullptr );
#else
    return readlink( path, out_full_path, max_size );
#endif // _WIN64
}

void file_directory_from_path( char* path ) {
    char* last_point = strrchr( path, '.' );
    char* last_separator = strrchr( path, '/' );
    if ( last_separator != nullptr && last_point > last_separator ) {
        *(last_separator + 1) = 0;
    }
    else {
        // Try searching backslash
        last_separator = strrchr( path, '\\' );
        if ( last_separator != nullptr && last_point > last_separator ) {
            *( last_separator + 1 ) = 0;
        }
        else {
            // Wrong input!
            iassertm( false, "Malformed path %s!", path );
        }

    }
}

void file_name_from_path( char* path ) {
    char* last_separator = strrchr( path, '/' );
    if ( last_separator == nullptr ) {
        last_separator = strrchr( path, '\\' );
    }

    if ( last_separator != nullptr ) {
        sizet name_length = strlen( last_separator + 1 );

        memcpy( path, last_separator + 1, name_length );
        path[ name_length ] = 0;
    }
}

StringView file_extension_from_path( StringView path ) {
    cstring last_separator = strrchr( path.data, '.' );

    return { last_separator + 1, path.size - ( last_separator - path.data ) };
}

bool fs_file_exists( StringView path ) {
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA( path.data, GetFileExInfoStandard, &unused );
#else
    int result = access( path.data, F_OK );
    return ( result == 0 );
#endif // _WIN64
}

bool fs_file_delete( StringView path ) {
#if defined(_WIN64)
    int result = remove( path.data );
    return result != 0;
#else
    int result = remove( path.data );
    return ( result == 0 );
#endif
}


bool fs_directory_exists( StringView path ) {
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA( path.data, GetFileExInfoStandard, &unused );
#else
    int result = access( path.data, F_OK );
    return ( result == 0 );
#endif // _WIN64
}

bool fs_directory_create( StringView path ) {
#if defined(_WIN64)
    int result = CreateDirectoryA( path.data, NULL );
    return result != 0;
#else
    int result = mkdir( path.data, S_IRWXU | S_IRWXG );
    return ( result == 0 );
#endif // _WIN64
}

bool fs_directory_delete( StringView path ) {
#if defined(_WIN64)
    int result = RemoveDirectoryA( path.data );
    return result != 0;
#else
    int result = rmdir( path.data );
    return ( result == 0 );
#endif // _WIN64
}

void fs_directory_current( Directory* directory ) {
#if defined(_WIN64)
    DWORD written_chars = GetCurrentDirectoryA( k_max_path, directory->path );
    directory->path[ written_chars ] = 0;
#else
    getcwd( directory->path, k_max_path );
#endif // _WIN64
}

void fs_directory_change( StringView path ) {
#if defined(_WIN64)
    if ( !SetCurrentDirectoryA( path.data ) ) {
        ilog( "Cannot change current directory to %s\n", path.data );
    }
#else
    if ( chdir( path.data ) != 0 ) {
        ilog( "Cannot change current directory to %s\n", path.data );
    }
#endif // _WIN64
}

//
static bool string_ends_with_char( cstring s, char c ) {
    cstring last_entry = strrchr( s, c );
    const sizet index = last_entry - s;
    return index == (strlen( s ) - 1);
}

void fs_open_directory( cstring path, Directory* out_directory ) {

    // Open file trying to conver to full path instead of relative.
    // If an error occurs, just copy the name.
    if ( fs_file_resolve_to_full_path( path, out_directory->path, MAX_PATH ) == 0 ) {
        strcpy( out_directory->path, path );
    }

    // Add '\\' if missing
    if ( !string_ends_with_char( path, '\\' ) ) {
        strcat( out_directory->path, "\\" );
    }

    if ( !string_ends_with_char( out_directory->path, '*' ) ) {
        strcat( out_directory->path, "*" );
    }

#if defined(_WIN64)
    out_directory->os_handle = nullptr;

    WIN32_FIND_DATAA find_data;
    HANDLE found_handle;
    if ( (found_handle = FindFirstFileA( out_directory->path, &find_data )) != INVALID_HANDLE_VALUE ) {
        out_directory->os_handle = found_handle;
    }
    else {
        ilog("Could not open directory %s\n", out_directory->path );
    }
#else
    iassertm( false, "Not implemented" );
#endif
}

void fs_close_directory( Directory* directory ) {
#if defined(_WIN64)
    if ( directory->os_handle ) {
        FindClose( directory->os_handle );
    }
#else
    iassertm( false, "Not implemented" );
#endif
}

void fs_parent_directory( Directory* directory ) {

    Directory new_directory;

    const char* last_directory_separator = strrchr( directory->path, '\\' );
    sizet index = last_directory_separator - directory->path;

    if ( index > 0 ) {

        strncpy( new_directory.path, directory->path, index );
        new_directory.path[index] = 0;

        last_directory_separator = strrchr( new_directory.path, '\\' );
        sizet second_index = last_directory_separator - new_directory.path;

        if ( last_directory_separator ) {
            new_directory.path[second_index] = 0;
        }
        else {
            new_directory.path[index] = 0;
        }

        fs_open_directory( new_directory.path, &new_directory );

#if defined(_WIN64)
        // Update directory
        if ( new_directory.os_handle ) {
            *directory = new_directory;
        }
#else
        iassertm( false, "Not implemented" );
#endif
    }
}

void fs_sub_directory( Directory* directory, cstring sub_directory_name ) {

    // Remove the last '*' from the path. It will be re-added by the file_open.
    if ( string_ends_with_char( directory->path, '*' ) ) {
        directory->path[strlen( directory->path ) - 1] = 0;
    }

    strcat( directory->path, sub_directory_name );
    fs_open_directory( directory->path, directory );
}

void fs_find_files_in_path( cstring file_pattern, StringArray& files ) {

    files.clear();

#if defined(_WIN64)
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if ( (hFind = FindFirstFileA( file_pattern, &find_data )) != INVALID_HANDLE_VALUE ) {
        do {

            files.intern( find_data.cFileName );

        } while ( FindNextFileA( hFind, &find_data ) != 0 );
        FindClose( hFind );
    }
    else {
        ilog( "Cannot find file %s\n", file_pattern );
    }
#else
    iassertm( false, "Not implemented" );
    // TODO(marco): opendir, readdir
#endif
}

void fs_find_files_in_path( cstring extension, cstring search_pattern, StringArray& files, StringArray& directories ) {

    files.clear();
    directories.clear();

    const bool all_files = strlen( extension ) == 1 && (extension[ 0 ] == '*');

#if defined(_WIN64)
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if ( (hFind = FindFirstFileA( search_pattern, &find_data )) != INVALID_HANDLE_VALUE ) {
        do {
            if ( find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                directories.intern( find_data.cFileName );
            }
            else {
                // If filename contains the extension, add it
                if ( strstr( find_data.cFileName, extension ) || all_files ) {
                    files.intern( find_data.cFileName );
                }
            }

        } while ( FindNextFileA( hFind, &find_data ) != 0 );
        FindClose( hFind );
    }
    else {
        ilog( "Cannot find directory %s\n", search_pattern );
    }
#else
    iassertm( false, "Not implemented" );
#endif
}

void environment_variable_get( cstring name, char* output, u32 output_size ) {
#if defined(_WIN64)
    ExpandEnvironmentStringsA( name, output, output_size );
#else
    cstring real_output = getenv( name );
    strncpy( output, real_output, output_size );
#endif
}

} // namespace idra
