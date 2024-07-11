/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "asset_compiler.hpp"
#include <stdio.h>

#include "kernel/array.hpp"
#include "kernel/log.hpp"
#include "kernel/assert.hpp"
#include "kernel/file.hpp"
#include "kernel/allocator.hpp"
#include "kernel/memory.hpp"
#include "kernel/string.hpp"
#include "kernel/utf.hpp"

#include <filesystem>
#include <cstdlib>

// Need this here as well.
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "external/stb_truetype.h"

#include "external/json.hpp"

namespace idra {

// Forward declarations ///////////////////////////////////////////////////
static void compile_animations( BookmarkAllocator* allocator, StringView source, StringView destination );
static void compile_atlas( BookmarkAllocator* allocator, StringView source, StringView destination );
static void compile_font( BookmarkAllocator* allocator, StringView source, StringView destination );
static void compile_texture( BookmarkAllocator* allocator, StringView source, StringView destination );
static void compile_ui( BookmarkAllocator* allocator, StringView source, StringView destination );

static void sanitize_path( char* path, sizet size );
static void copy_path( cstring path, sizet size, char* destination_path );

void copy_path( char* destination_path, const std::filesystem::path& source_path, sizet size ) {
#if defined (_MSC_VER)
    wcstombs( destination_path, source_path.c_str(), size );
#else
    const sizet source_path_length = strlen( source_path.c_str() );
    sizet max_length = source_path_length < size - 1 ? source_path_length : size - 1;
    memcpy( destination_path, source_path.c_str(), max_length );
    destination_path[ max_length ] = 0;
#endif // _MSC_VER
}

// Main ///////////////////////////////////////////////////////////////////
void asset_compiler_main( StringView source_folder, StringView destination_folder ) {

    // Setup global allocator for external allocations
    MallocAllocator mallocator;
    g_memory->set_current_allocator( &mallocator );

    TLSFAllocator tlsf_allocator;
    tlsf_allocator.init( imega( 1 ) );

    BookmarkAllocator bookmark_allocator;
    bookmark_allocator.init( &tlsf_allocator, ikilo( 900 ), "Asset Compiler Allocator" );

    BookmarkAllocator* allocator = &bookmark_allocator;
    StringBuffer names_buffer;
    names_buffer.init( ikilo( 1 ), allocator );

    // Create destination folder if not present
    fs_directory_create( destination_folder );

    // First collect all the folders and create them in the destination folder.
    for ( std::filesystem::recursive_directory_iterator i( source_folder.data ), end; i != end; ++i ) {
        if ( std::filesystem::is_directory( i->path() ) ) {
            //ilog( "%S\n", i->path().relative_path().c_str() );

            char relative_path[ 512 ];
            //wcstombs( relative_path, i->path().relative_path().c_str(), 512 );
            copy_path( relative_path, i->path().relative_path(), 512 );
            sanitize_path( relative_path, strlen( relative_path ) );

            cstring subpath = relative_path + source_folder.size + 1; // + 1 for the separator

            names_buffer.clear();
            StringView path_name = names_buffer.append_use_f( "%s/%s", destination_folder.data, subpath );
            if ( !fs_directory_exists( path_name ) ) {

                fs_directory_create( path_name );

                ilog_warn( "Creating destination directory %s\n", path_name.data );
            }


            //ilog( "Path: %s, subpath %s, %s\n", relative_path, subpath, path_name.data );
        }
    }

    // Buffers used to convert from wide char to char
    char source_parent_path[ 512 ];
    char extension[ 32 ];
    char filename[ 256 ];

    // Then scan each source file and compile to its binary form.
    for ( std::filesystem::recursive_directory_iterator i( source_folder.data ), end; i != end; ++i ) {
        if ( !std::filesystem::is_directory( i->path() ) ) {
            // Use the capital 'S' to print wchars
            //ilog( "%S\n", i->path().filename().c_str() );
            //ilog( "%S\n", i->path().relative_path().c_str() );
            //ilog( "%S\n", i->path().parent_path().c_str() );
            //ilog( "%S\n", i->path().extension().c_str() );
            //ilog( "%S\n", i->path().root_path().c_str() );

            // Convert full path from wchar to char
            //wcstombs( source_parent_path, i->path().parent_path().c_str(), 512 );
            copy_path( source_parent_path, i->path().parent_path(), 512 );
            sanitize_path( source_parent_path, strlen( source_parent_path ) );

            // Convert extension
            //wcstombs( extension, i->path().extension().c_str(), 32 );
            copy_path( extension, i->path().extension(), 32 );

            // Convert filename
            //wcstombs( filename, i->path().filename().c_str(), 256 );
            copy_path( filename, i->path().filename(), 256 );
            // Remove extension from filename
            sizet filename_extension_index = strlen( filename ) - strlen( extension );
            if ( filename_extension_index < strlen( filename ) ) {
                filename[ filename_extension_index ] = 0;
            }

            // Extract the subpath without the source folder
            cstring subpath = source_parent_path + source_folder.size;
            //ilog( "Path: %s, subpath %s, destination path %s\n", source_relative_path, subpath, destination_name.data );

            sizet extension_length = strlen( extension );
            if ( extension_length > 2 ) {

                names_buffer.clear();

                StringView source_path = names_buffer.append_use_f( "%s/%s%s", source_parent_path, filename, extension );

                switch ( extension[ 1 ] ) {

                    case 'a':
                    {
                        if ( extension[ 2 ] == 't' && extension[ 3 ] == 'j' ) {
                            StringView destination_path = names_buffer.append_use_f( "%s%s/%s.bhat", destination_folder.data, subpath, filename );
                            ilog( "Compiling %s into %s\n", source_path.data, destination_path.data );

                            compile_atlas( allocator, source_path, destination_path );
                        }
                    }

                    case 'c':
                    {
                        break;
                    }

                    case 'f':
                    {
                        break;
                    }

                    case 'h':
                    {
                        // HFX files
                        if ( extension[ 2 ] == 'f' && extension[ 3 ] == 'x' ) {

                            StringView destination_path = names_buffer.append_use_f( "%s/%s.bhfx", destination_folder.data, subpath );
                            ilog( "Compiling %s into %s\n", source_parent_path, destination_path.data );

                            //hfx::hfx_compile( source_name, destination_name, hfx::CompileOptions_VulkanStandard, "..//source//generated" );

                        } else if ( extension[ 2 ] == 'a' && extension[ 3 ] == 'j' ) {

                            StringView destination_path = names_buffer.append_use_f( "%s%s/%s.bha", destination_folder.data, subpath, filename );
                            ilog( "Compiling %s into %s\n", source_path.data, destination_path.data );

                            compile_animations( allocator, source_path, destination_path );

                        } else if ( extension[ 2 ] == 'i' && extension[ 3 ] == 'j' ) {

                            names_buffer.clear();
                            StringView destination_path = names_buffer.append_use_f( "%s/%s.bhi", destination_folder.data, subpath );
                            ilog( "Compiling %s into %s\n", source_parent_path, destination_path.data );

                            //compile_input( &allocator, source_relative_path, destination_path );
                        }
                        break;
                    }

                    case 'p':
                    {
                        bool is_png = extension[ 2 ] == 'n' && extension[ 3 ] == 'g';
                        bool is_pgm = extension[ 2 ] == 'g' && extension[ 3 ] == 'm';
                        if ( is_png || is_pgm ) {

                            // Just copy the filename with extension as well for now.
                            StringView destination_path = names_buffer.append_use_f( "%s%s/%s%s", destination_folder.data, subpath, filename, extension );

                            ilog( "Compiling %s into %s\n", source_path.data, destination_path.data );
                            compile_texture( allocator, source_path, destination_path );
                        }

                        break;
                    }

                    case 'r':
                    {
                        if ( extension[ 2 ] == 'a' && extension[ 3 ] == 'w' ) {

                            // Just copy the filename with extension as well for now.
                            StringView destination_path = names_buffer.append_use_f( "%s%s/%s%s", destination_folder.data, subpath, filename, extension );

                            ilog( "Compiling %s into %s\n", source_path.data, destination_path.data );
                            compile_texture( allocator, source_path, destination_path );
                        }

                        break;
                    }

                    case 'u':
                    {
                        if ( extension[ 2 ] == 'i' && extension[ 3 ] == 'j' ) {

                            StringView destination_path = names_buffer.append_use_f( "%s%s/%s.bui", destination_folder.data, subpath, filename );
                            ilog( "Compiling %s into %s\n", source_path.data, destination_path.data );

                            compile_ui( allocator, source_path, destination_path );
                        }
                        break;
                    }


                    default:
                    {
                        ilog_warn( "Skipping file %s\n", source_path.data );
                        break;
                    }
                }
            }
        }
    }

    names_buffer.shutdown();

    bookmark_allocator.shutdown();
    tlsf_allocator.shutdown();

    g_memory->set_current_allocator( nullptr );
}

void sanitize_path( char* path, sizet size ) {

    for ( sizet i = 0; i < size; ++i ) {
        if ( path[ i ] == '\\' ) {
            path[ i ] = '/';
        }
    }
}

// Compilation methods
void compile_animations( BookmarkAllocator* allocator, StringView source, StringView destination ) {

    using json = nlohmann::json;

    u64 marker = allocator->get_marker();

    Span<char> file_data = file_read_allocate( source, allocator );
    if ( file_data.size == 0 ) {
        ilog_error( "Could not read file %s\n", source.data );
    }

    json parsed_json = json::parse( file_data.data );

    std::string name_str;
    parsed_json[ "texture" ].get_to( name_str );

    // Name null or not valid
    if ( name_str.size() < 4 ) {
        ilog_error( "Invalid texture name %s\n", name_str.c_str() );
        return;
    }

    i32 comp, width, height;
    i32 image_load_result = stbi_info( name_str.c_str(), &width, &height, &comp );
    if ( !image_load_result ) {
        ilog_error( "Error loading texture %s\n", name_str.c_str() );
        return;
    }

    // Calculate total size of memory blob
    sizet blob_size = sizeof( SpriteAnimationBlueprint ) + 16;

    json animation_array = parsed_json[ "animations" ];
    const u32 num_animations = ( u32 )animation_array.size();
    blob_size += sizeof( SpriteAnimationCreation ) * num_animations;

    // Create memory blob
    BlobWriter writer;
    SpriteAnimationBlueprint* blueprint = writer.write<SpriteAnimationBlueprint>(
        allocator, SpriteAnimationBlueprint::k_version, blob_size );

    writer.reserve_and_set( blueprint->animations, num_animations );

    for ( u32 i = 0; i < num_animations; ++i ) {
        json animation = animation_array[ i ];
        SpriteAnimationCreation& ac = blueprint->animations[ i ];

        ac.texture_width = width;
        ac.texture_height = height;
        ac.offset_x = animation.value( "start_x", 0 );
        ac.offset_y = animation.value( "start_y", 0 );
        ac.frame_width = animation.value( "width", 1 );
        ac.frame_height = animation.value( "height", 1 );
        ac.num_frames = animation.value( "num_frames", 1 );
        ac.columns = animation.value( "columns", 1 );
        ac.fps = animation.value( "fps", 8 );
        ac.looping = animation.value( "looping", false );
        ac.invert = animation.value( "invert", false );
        // TODO: frame table backed
        ac.frame_table_ = {};
    }

    // Write to file
    const u32 blueprint_size = writer.reserved_offset;

    FileHandle f = file_open_for_write( destination );
    fwrite( ( const void* )blueprint, blueprint_size, 1, f );
    file_close( f );

    allocator->free_marker( marker );
}

void compile_atlas( BookmarkAllocator* allocator, StringView source, StringView destination ) {

    // Read json file
    using json = nlohmann::json;

    sizet current_allocator_marker = allocator->get_marker();

    StringBuffer string_buffer;
    string_buffer.init( 1024, allocator );

    Span<char> file_data = file_read_allocate( source, allocator );

    if ( file_data.data ) {
        json json_data = json::parse( file_data.data );

        json texture_name = json_data[ "texture" ];
        std::string name_string;

        if ( !texture_name.is_string() ) {
            ilog_error( "Error no texture specified in atlas %s", source.data );
            return;
        }

        texture_name.get_to( name_string );
        ilog_debug( "Atlas %s references texture %s\n", source.data, name_string.c_str() );

        // Build dependencies
        // TODO:
        //build_texture( name_string.c_str() );

        i32 comp, width, height;
        i32 image_load_result = stbi_info( name_string.c_str(), &width, &height, &comp );
        if ( !image_load_result ) {
            ilog_error( "Error loading texture %s", name_string.c_str() );
            return;
        }

        json regions = json_data[ "regions" ];

        // Build atlas
        BlobWriter writer;
        AtlasBlueprint* atlas_blueprint = writer.write<AtlasBlueprint>( allocator, AtlasBlueprint::k_version, 1000 );

        if ( regions.is_array() ) {
            const sizet region_count = regions.size();
            writer.reserve_and_set( atlas_blueprint->entries, region_count );
            writer.reserve_and_set( atlas_blueprint->entry_names, region_count );

            std::string region_name_string;

            for ( sizet i = 0; i < region_count; ++i ) {
                json region = regions[ i ];
                AtlasEntry& entry = atlas_blueprint->entries[ i ];

                entry.uv_offset_x = region.value( "x", 0.0f ) / width;
                entry.uv_offset_y = region.value( "y", 0.0f ) / height;
                entry.uv_width = region.value( "width", 0.0f ) / width;
                entry.uv_height = region.value( "height", 0.0f ) / height;

                json region_name = region[ "name" ];
                if ( region_name.is_string() ) {
                    region_name.get_to( region_name_string );

                    StringView name_string_view{ region_name_string.c_str(), region_name_string.length() };
                    writer.reserve_and_set( atlas_blueprint->entry_names[ i ], name_string_view );
                } else {
                    static cstring no_name_entry = "no_name_entry";
                    StringView name_string_view( no_name_entry );

                    writer.reserve_and_set( atlas_blueprint->entry_names[ i ], name_string_view );
                }
            }
        }
#if defined ( IDRA_USE_COMPRESSED_TEXTURES )
        // Write texture name with different extension (.bin)
        char* name_cur = string_buffer.current();
        // TODO: get extension
        string_buffer.append_m( (void*)name_string.c_str(), name_string.length() - 3 );
        string_buffer.append_f( "bin" );
        string_buffer.close_current_string();
        StringView name_string_view{ name_cur, name_string.length() };
#else
        StringView name_string_view{ name_string.c_str(), name_string.length() };
#endif // IDRA_USE_COMPRESSED_TEXTURES
        writer.reserve_and_set( atlas_blueprint->texture_name, name_string_view );

        // Write to file
        const u32 blueprint_size = writer.reserved_offset;

        FileHandle f = file_open_for_write( destination );
        fwrite( ( const void* )atlas_blueprint, blueprint_size, 1, f );
        file_close( f );
    }

    // Free memory
    allocator->free_marker( current_allocator_marker );
}

void compile_texture( BookmarkAllocator* allocator, StringView source, StringView destination ) {

#if defined ( IDRA_USE_COMPRESSED_TEXTURES )
    // Check if blueprint file exists
    const bool destination_file_exists = fs_file_exists( destination );

    // If exists, check if source size and source data are the same
    //if ( destination_file_exists ) {
    //    FileHandle texture_file = file_open_for_read( destination );
    //    // do not need to check that files exists

    //    // TODO:
    //    // Fill just the blueprint
    //    TextureBlueprint blueprint;
    //    Span<char> blueprint_memory( ( char* )&blueprint, sizeof( TextureBlueprint ) );

    //    sizet read_bytes = file_read( texture_file, blueprint_memory, sizeof( TextureBlueprint ) );
    //    if ( read_bytes >= sizeof( TextureBlueprint ) ) {
    //        // TODO
    //        iassert( false );
    //    }
    //}

    i32 width, height, components;

    // TODO: memory management ?
    u8* texture_memory = stbi_load( source.data, &width, &height, &components, 4 );
    if ( !texture_memory ) {
        ilog_error( "Failed loading texture %s\n", source.data );

        return;
    }

    const sizet texture_size = width * height * components;
    const sizet blob_size = texture_size + sizeof( TextureBlueprint ) + destination.size;
    MallocAllocator mallocator;
    // Build atlas
    BlobWriter writer;
    TextureBlueprint* blueprint = writer.write<TextureBlueprint>( &mallocator, TextureBlueprint::k_version, blob_size );

    blueprint->gpu_creation = { .width = ( u16 )width, .height = ( u16 )height, .depth = 1, .array_layer_count = 1,
                                  .mip_level_count = 1, .flags = TextureFlags::Default_mask,
                                  .format = TextureFormat::R8G8B8A8_UNORM, .type = TextureType::Texture2D,
                                  .initial_data = nullptr, .debug_name = {} };

    // Patch initial data and debug name later.
    // Write name
    writer.reserve_and_set( blueprint->name, destination );

    // Write actual texture memory
    writer.reserve_and_set( blueprint->texture_data, texture_size );
    memcpy( blueprint->texture_data.get(), texture_memory, texture_size );

    const u32 blueprint_size = writer.reserved_offset;

    FileHandle f = file_open_for_write( destination );
    fwrite( ( const void* )blueprint, blueprint_size, 1, f );
    file_close( f );

#else
    // If they differ, build the file.

    const bool files_have_different_sizes = fs_file_get_size( source ) != fs_file_get_size( destination );
    const bool files_have_different_time = fs_file_last_write_time( source ) != fs_file_last_write_time( destination );
    // TODO: For now always copy
    if ( files_have_different_time || files_have_different_sizes ) {
        if ( !fs_file_copy( source, destination ) ) {
            ilog_error( "Could not copy file %s\n", destination.data );
        }
    } else {
        ilog( "Files are the same, skipping copy...\n" );
    }
#endif // IDRA_USE_COMPRESSED_TEXTURES
}

void compile_ui( BookmarkAllocator* allocator, StringView source, StringView destination ) {

    // Read json file
    using json = nlohmann::json;

    sizet current_allocator_marker = allocator->get_marker();

    StringBuffer string_buffer;
    string_buffer.init( 1024, allocator );

    Span<char> file_data = file_read_allocate( source, allocator );

    if ( file_data.data ) {
        json json_data = json::parse( file_data.data );

        json texture_name = json_data[ "texture" ];
        std::string name_string;

        if ( !texture_name.is_string() ) {
            ilog_error( "Error no texture specified in atlas %s", source.data );
            return;
        }

        texture_name.get_to( name_string );
        ilog_debug( "Atlas %s references texture %s\n", source.data, name_string.c_str() );

        // Build dependencies
        // TODO:
        //build_texture( name_string.c_str() );

        i32 comp, width, height;
        auto a = g_memory->get_current_allocator();
        i32 image_load_result = stbi_info( name_string.c_str(), &width, &height, &comp );
        if ( !image_load_result ) {
            ilog_error( "Error loading texture %s", name_string.c_str() );
            return;
        }

        json text_frame = json_data[ "text_frame" ];

        // Build atlas
        BlobWriter writer;
        UIBlueprint* blueprint = writer.write<UIBlueprint>( allocator, UIBlueprint::k_version, 1000 );

        if ( text_frame.is_array() ) {
            const sizet region_count = text_frame.size();
            writer.reserve_and_set( blueprint->entry_names, region_count );

            std::string region_name_string;

            for ( sizet i = 0; i < region_count; ++i ) {
                json region = text_frame[ i ];
                UITextFrameEntry& entry = blueprint->text_frame_elements[ i ];

                entry.uv_offset_x = region.value( "x", 0.0f ) / width;
                entry.uv_offset_y = region.value( "y", 0.0f ) / height;
                entry.uv_width = region.value( "width", 0.0f ) / width;
                entry.uv_height = region.value( "height", 0.0f ) / height;
                entry.position_offset_x = region.value( "offset_x", 0.0f );
                entry.position_offset_y = region.value( "offset_y", 0.0f );

                json region_name = region[ "name" ];
                if ( region_name.is_string() ) {
                    region_name.get_to( region_name_string );

                    StringView name_string_view{ region_name_string.c_str(), region_name_string.length() };
                    writer.reserve_and_set( blueprint->entry_names[ i ], name_string_view );
                } else {
                    static cstring no_name_entry = "no_name_entry";
                    StringView name_string_view( no_name_entry );

                    writer.reserve_and_set( blueprint->entry_names[ i ], name_string_view );
                }
            }
        }

        StringView name_string_view{ name_string.c_str(), name_string.length() };
        writer.reserve_and_set( blueprint->texture_name, name_string_view );

        // Write to file
        const u32 blueprint_size = writer.reserved_offset;

        FileHandle f = file_open_for_write( destination );
        fwrite( ( const void* )blueprint, blueprint_size, 1, f );
        file_close( f );
    }

    // Free memory
    allocator->free_marker( current_allocator_marker );
}

void compile_font( BookmarkAllocator* allocator, StringView source, StringView destination ) {
}

} // namespace idra
