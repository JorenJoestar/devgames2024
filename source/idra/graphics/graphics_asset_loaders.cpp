/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "graphics/graphics_asset_loaders.hpp"

#include "kernel/file.hpp"
#include "kernel/memory_hooks.hpp"

#include "gpu/gpu_device.hpp"

#include "graphics/graphics_blueprints.hpp"

#include "tools/shader_compiler/shader_compiler.hpp"

#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "external/stb_truetype.h"


namespace idra {

// ShaderAssetLoader //////////////////////////////////////////////////////
void ShaderAssetLoader::init( Allocator* allocator, u32 size, AssetManager* asset_manager, GpuDevice* gpu ) {
    AssetLoader<ShaderAsset>::init( allocator, size, asset_manager );

    gpu_device = gpu;

    string_array.init( ikilo( 128 ), allocator );
    shader_creations.init( allocator, 32 );
}

void ShaderAssetLoader::shutdown() {
    AssetLoader<ShaderAsset>::shutdown();

    string_array.shutdown();
    shader_creations.shutdown();
}

ShaderAsset* ShaderAssetLoader::load( StringView name ) {
    ShaderAsset* shader = path_to_asset.get( hash_calculate( name ) );

    if ( shader ) {
        shader->reference_count++;
        return shader;
    }

    ilog_error( "Could not find shader %s\n", name.data );
    return nullptr;
}

void ShaderAssetLoader::unload( StringView name ) {
    const u64 hashed_name = hash_calculate( name );
    ShaderAsset* shader = path_to_asset.get( hashed_name );

    unload( shader );
}

void ShaderAssetLoader::unload( ShaderAsset* shader ) {
    if ( shader ) {
        shader->reference_count--;

        if ( shader->reference_count == 0 ) {
            gpu_device->destroy_shader_state( shader->shader );

            const u64 hashed_path = hash_calculate( shader->path.path );
            path_to_asset.remove( hashed_path );
            assets.release( shader );
            asset_manager->free_path( shader->path );
        }
    }
}

void ShaderAssetLoader::reload_assets() {

    // Reload and substitute shader states
    FlatHashMapIterator it = path_to_asset.iterator_begin();
    while ( it.is_valid() ) {

        auto key_value = path_to_asset.get_structure( it );
        const u64 key = key_value.key;
        ShaderAsset* shader = key_value.value;

        // Compile shader and substitute handle
        ShaderState* shader_state = gpu_device->shader_states.get_cold( shader->shader );
        
        if ( shader_state ) {
            switch ( shader_state->pipeline_type ) {
                case PipelineType::Graphics:
                {
                    ShaderAssetCreation& vs_creation = shader_creations[ shader->creation_index ];
                    // TODO: always true ?
                    ShaderAssetCreation& fs_creation = shader_creations[ shader->creation_index + 1];

                    std::vector<unsigned int> vs_spirv, fs_spirv;
                    // TODO: includes are common between vertex and fragment shaders for now.
                    StringView includes[ 8 ];
                    for ( u32 i = 0; i < vs_creation.num_includes; ++i ) {
                        includes[ i ] = vs_creation.includes[ i ];
                    }

                    StringView defines[ 8 ];
                    for ( u32 i = 0; i < vs_creation.num_defines; ++i ) {
                        defines[ i ] = vs_creation.defines[ i ];
                    }

                    shader_compiler_compile_from_file( { .defines = Span<const StringView>( defines, vs_creation.num_defines ),
                                                       .include_paths = Span<const StringView>( includes, vs_creation.num_includes ),
                                                       .source_path =  vs_creation.source_path, .stage = ShaderStage::Vertex }, vs_spirv );

                    shader_compiler_compile_from_file( { .defines = Span<const StringView>( defines, vs_creation.num_defines ),
                                                       .include_paths = Span<const StringView>( includes, fs_creation.num_includes ),
                                                       .source_path =  fs_creation.source_path, .stage = ShaderStage::Fragment }, fs_spirv );

                    // Compilation succeeded, create new shader state and substitute the one in the asset.
                    if ( vs_spirv.size() != 0 && fs_spirv.size() != 0 ) {
                        ShaderStateHandle new_shader_state = gpu_device->create_graphics_shader_state( {
                            .vertex_shader = {
                                .byte_code = Span<u32>( vs_spirv.data(), vs_spirv.size() * 4 ),
                                .type = ShaderStage::Vertex } ,
                            .fragment_shader = {
                                .byte_code = Span<u32>( fs_spirv.data(), fs_spirv.size() * 4 ),
                                .type = ShaderStage::Fragment } ,
                            .debug_name = vs_creation.name } );

                        gpu_device->destroy_shader_state( shader->shader );

                        shader->shader = new_shader_state;
                    }

                    break;
                }

                case PipelineType::Compute:
                {
                    ShaderAssetCreation& creation = shader_creations[ shader->creation_index ];
                    
                    std::vector<unsigned int> spirv;
                    
                    StringView includes[ 8 ];
                    for ( u32 i = 0; i < creation.num_includes; ++i ) {
                        includes[ i ] = creation.includes[ i ];
                    }

                    shader_compiler_compile_from_file( { .defines = {}, .include_paths = Span<const StringView>( includes, creation.num_includes ),
                                                       .source_path =  creation.source_path, .stage = ShaderStage::Compute }, spirv );

                    // Compilation succeeded, create new shader state and substitute the one in the asset.
                    if ( spirv.size() != 0 ) {
                        ShaderStateHandle new_shader_state = gpu_device->create_compute_shader_state( {
                            .compute_shader = {
                                .byte_code = Span<u32>( spirv.data(), spirv.size() * 4 ),
                                .type = ShaderStage::Compute } ,
                            .debug_name = creation.name } );

                        gpu_device->destroy_shader_state( shader->shader );

                        shader->shader = new_shader_state;
                    }

                    break;
                }

                case PipelineType::Raytracing:
                default:
                {
                    iassertm( false, "Pipeline not supported!\n" );
                    break;
                }
            }
        }
        
        path_to_asset.iterator_advance( it );
    }
}

u32 ShaderAssetLoader::cache_creation_info( Span<const StringView> defines, 
                                            Span<const StringView> include_paths, StringView path,
                                            ShaderStage::Enum stage, StringView name ) {
    ShaderAssetCreation& creation = shader_creations.push_use();

    creation.num_defines = 0;

    for ( sizet i = 0; i < defines.size; ++i ) {
        creation.defines[ creation.num_defines++ ] = string_array.intern( defines[ i ].data );
    }
    
    creation.num_includes = 0;
    // Cache includes and interns the strings to create the views
    for ( sizet i = 0; i < include_paths.size; ++i ) {
        creation.includes[ creation.num_includes++ ] = string_array.intern( include_paths[ i ].data );
    }

    creation.source_path = string_array.intern( path.data );
    creation.stage = stage;
    creation.name = string_array.intern( name.data );

    return shader_creations.size - 1;
}

ShaderAsset* ShaderAssetLoader::compile_graphics( Span<const StringView> defines,
                                                  Span<const StringView> includes,
                                                  StringView vertex_path,
                                                  StringView fragment_path,
                                                  StringView name ) {
    const u64 hashed_name = hash_calculate( name );
    ShaderAsset* shader = path_to_asset.get( hashed_name );

    if ( shader ) {
        shader->reference_count++;
        return shader;
    }

    std::vector<unsigned int> vs_spirv, fs_spirv;
    // Shader compiler DLL interaction.
    shader_compiler_compile_from_file( { .defines = defines, .include_paths = includes, .source_path =  vertex_path, .stage = ShaderStage::Vertex }, vs_spirv );
    shader_compiler_compile_from_file( { .defines = defines, .include_paths = includes, .source_path =  fragment_path, .stage = ShaderStage::Fragment }, fs_spirv );

    if ( vs_spirv.size() == 0 || fs_spirv.size() == 0 ) {
        ilog_error( "Error compiling shader %s\n", name.data );
        return nullptr;
    }

    ShaderStateHandle shader_state = gpu_device->create_graphics_shader_state( {
        .vertex_shader = {
            .byte_code = Span<u32>( vs_spirv.data(), vs_spirv.size() * 4 ),
            .type = ShaderStage::Vertex } ,
        .fragment_shader = {
            .byte_code = Span<u32>( fs_spirv.data(), fs_spirv.size() * 4 ),
            .type = ShaderStage::Fragment } ,
        .debug_name = name } );

    shader = assets.obtain();
    iassert( shader );

    shader->path = asset_manager->allocate_path( name );
    shader->shader = shader_state;
    shader->reference_count = 1;

    path_to_asset.insert( hashed_name, shader );

    // Cache shader creation infos
    shader->creation_count = 2;
    shader->creation_index = cache_creation_info( defines, includes, vertex_path, ShaderStage::Vertex, name );
    // TODO: always subsequent index is valid ?
    cache_creation_info( defines, includes, fragment_path, ShaderStage::Fragment, name );

    return shader;
}

ShaderAsset* ShaderAssetLoader::compile_compute( Span<const StringView> defines,
                                                 Span<const StringView> includes,
                                                 StringView path,
                                                 StringView name ) {

    const u64 hashed_name = hash_calculate( name );
    ShaderAsset* shader = path_to_asset.get( hashed_name );

    if ( shader ) {
        shader->reference_count++;
        return shader;
    }

    std::vector<unsigned int> spirv;
    // Shader compiler DLL interaction.
    shader_compiler_compile_from_file( { .defines = defines, .include_paths = includes, 
                                       .source_path =  path, .stage = ShaderStage::Compute }, spirv );

    if ( spirv.size() == 0 ) {
        ilog_error( "Error compiling shader %s\n", name.data );
        return nullptr;
    }

    ShaderStateHandle shader_state = gpu_device->create_compute_shader_state( {
        .compute_shader = {
            .byte_code = Span<u32>( spirv.data(), spirv.size() * 4 ),
            .type = ShaderStage::Compute } ,
        .debug_name = name } );

    shader = assets.obtain();
    iassert( shader );

    shader->path = asset_manager->allocate_path( name );
    shader->shader = shader_state;
    shader->reference_count = 1;

    path_to_asset.insert( hashed_name, shader );

    // Cache shader creation infos
    shader->creation_count = 1;
    shader->creation_index = cache_creation_info( defines, includes, path, ShaderStage::Compute, name );

    return shader;
}

// TextureAssetLoader /////////////////////////////////////////////////////

void TextureAssetLoader::init( Allocator* allocator, u32 size, AssetManager* asset_manager, GpuDevice* gpu ) {
    AssetLoader<TextureAsset>::init( allocator, size, asset_manager );

    gpu_device = gpu;
}

void TextureAssetLoader::shutdown() {
    AssetLoader<TextureAsset>::shutdown();
}

TextureAsset* TextureAssetLoader::load( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    TextureAsset* texture = path_to_asset.get( hashed_path );

    if ( texture ) {
        texture->reference_count++;
        return texture;
    }

    texture = assets.obtain();
    iassert( texture );

    texture->reference_count = 1;

#if defined ( IDRA_USE_COMPRESSED_TEXTURES )
    // TODO:
    Allocator* allocator = g_memory->get_resident_allocator();

    Span<char> file = file_read_allocate( path, allocator );

    if ( !file.data ) {
        ilog_error( "Failed loading texture %s\n", path );
        return nullptr;
    }

    // Load from blueprint!
    BlobReader blob_reader;
    // TODO: allocation when versions are different ?
    TextureBlueprint* blueprint = blob_reader.read<TextureBlueprint>( nullptr, TextureBlueprint::k_version, file, false );
    // Patch pointers
    blueprint->gpu_creation.initial_data = blueprint->texture_data.get();
    blueprint->gpu_creation.debug_name = blueprint->name.c_str();

    texture->texture = gpu_device->create_texture( blueprint->gpu_creation );
    texture->blueprint = blueprint;
#else
    // Load texture from file
    i32 width, height, components;

    // TODO: memory management ?
    u8* texture_memory = stbi_load( path.data, &width, &height, &components, 4 );
    if( !texture_memory ) {
        ilog_error( "Failed loading texture %s\n", path.data );

        assets.release( texture );

        return nullptr;
    }
    
    texture->texture = gpu_device->create_texture( { .width = ( u16 )width, .height = ( u16 )height, .depth = 1, .array_layer_count = 1,
                                  .mip_level_count = 1, .flags = TextureFlags::Default_mask,
                                  .format = TextureFormat::R8G8B8A8_UNORM, .type = TextureType::Texture2D,
                                  .initial_data = texture_memory, .debug_name = path } );
    texture->texture_data = texture_memory;
#endif // IDRA_USE_COMPRESSED_TEXTURES
    
    texture->path = asset_manager->allocate_path( path );

    path_to_asset.insert( hashed_path, texture );

    return texture;
}

void TextureAssetLoader::unload( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    TextureAsset* texture = path_to_asset.get( hashed_path );

    unload( texture );
}

void TextureAssetLoader::unload( TextureAsset* texture ) {

    if ( texture ) {
        texture->reference_count--;

        if ( texture->reference_count == 0 ) {
            gpu_device->destroy_texture( texture->texture );

#if defined ( IDRA_USE_COMPRESSED_TEXTURES )
            if ( texture->blueprint ) {
                Allocator* allocator = g_memory->get_resident_allocator();
                ifree( texture->blueprint, allocator );
            }
#else
            if ( texture->texture_data ) {
                free( texture->texture_data );
            }
#endif // IDRA_USE_COMPRESSED_TEXTURES

            const u64 hashed_path = hash_calculate( texture->path.path );
            path_to_asset.remove( hashed_path );
            assets.release( texture );
            asset_manager->free_path( texture->path );
       }
    }
}

// TextureAtlasLoader /////////////////////////////////////////////////////
void TextureAtlasLoader::init( Allocator* allocator_, u32 size, AssetManager* asset_manager, GpuDevice* gpu_ ) {

    AssetLoader<AtlasAsset>::init( allocator_, size, asset_manager );

    gpu_device = gpu_;
    allocator = allocator_;
}

void TextureAtlasLoader::shutdown() {
    AssetLoader<AtlasAsset>::shutdown();
}

AtlasAsset* TextureAtlasLoader::load( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    AtlasAsset* atlas = path_to_asset.get( hashed_path );

    if ( atlas ) {
        atlas->reference_count++;
        return atlas;
    }

    atlas = assets.obtain();
    iassert( atlas );

    atlas->reference_count = 1;

    // Actually load the font from the ttf file and create a texture
    Span<char> atlas_file = file_read_allocate( path, allocator );

    if ( !atlas_file.data ) {
        ilog_error( "Failed loading atlas %s\n", path );
        return nullptr;
    }

    BlobReader blob_reader;
    // TODO: allocation when versions are different ?
    atlas->blueprint = blob_reader.read<AtlasBlueprint>( nullptr, AtlasBlueprint::k_version, atlas_file, false );
    atlas->path = asset_manager->allocate_path( path );

    // Load dependant resource
    atlas->texture = asset_manager->get_loader<TextureAssetLoader>()->load( atlas->blueprint->texture_name.c_str() );

    path_to_asset.insert( hashed_path, atlas );

    return atlas;
}

void TextureAtlasLoader::unload( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    AtlasAsset* asset = path_to_asset.get( hashed_path );

    unload( asset );
}

void TextureAtlasLoader::unload( AtlasAsset* asset ) {
    if ( asset ) {
        asset->reference_count--;

        if ( asset->reference_count == 0 ) {
            asset_manager->get_loader<TextureAssetLoader>()->unload( asset->blueprint->texture_name.c_str() );

            if ( asset->blueprint ) {
                ifree( asset->blueprint, allocator );
            }

            const u64 hashed_path = hash_calculate( asset->path.path );
            path_to_asset.remove( hashed_path );
            assets.release( asset );
            asset_manager->free_path( asset->path );
        }
    }
}

// SpriteAnimationAssetLoader /////////////////////////////////////////////

void SpriteAnimationAssetLoader::init( Allocator* allocator_, u32 size, AssetManager* asset_manager ) {
    allocator = allocator_;

    AssetLoader<SpriteAnimationAsset>::init( allocator, size, asset_manager );
}

void SpriteAnimationAssetLoader::shutdown() {
    AssetLoader<SpriteAnimationAsset>::shutdown();
}

SpriteAnimationAsset* SpriteAnimationAssetLoader::load( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    SpriteAnimationAsset* asset = path_to_asset.get( hashed_path );

    if ( asset ) {
        asset->reference_count++;
        return asset;
    }

    asset = assets.obtain();
    iassert( asset );

    asset->reference_count = 1;

    // Actual load
    Span<char> blob_memory = file_read_allocate( path, allocator );

    BlobReader blob_reader{};
    // TODO: force serialize for now.
    asset->blueprint = blob_reader.read<SpriteAnimationBlueprint>( allocator, SpriteAnimationBlueprint::k_version, blob_memory, false );

    // If reader has allocated memory, we can get rid of the initial blob memory as
    // the blueprint is living in the serialized data memory.
    if ( blob_reader.data_memory ) {
        ifree( blob_memory.data, allocator );
    }

    asset->path = asset_manager->allocate_path( path );

    path_to_asset.insert( hashed_path, asset );

    return asset;
}

void SpriteAnimationAssetLoader::unload( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    SpriteAnimationAsset* asset = path_to_asset.get( hashed_path );

    unload( asset );
}

void SpriteAnimationAssetLoader::unload( SpriteAnimationAsset* asset ) {

    if ( asset ) {
        asset->reference_count--;

        if ( asset->reference_count == 0 ) {
            // Always free the blueprint memory.
            if ( asset->blueprint ) {
                ifree( asset->blueprint, allocator );
            }

            const u64 hashed_path = hash_calculate( asset->path.path );
            path_to_asset.remove( hashed_path );
            assets.release( asset );

            asset_manager->free_path( asset->path );
        }
    }
}


static int calculate_bitmap_width( const stbtt_fontinfo& info, int line_height ) {
    const f32 scale = stbtt_ScaleForPixelHeight( &info, line_height * 1.0f );

    i32 width = 0;

    for ( char c = 32; c < 127; ++c ) {
        i32 advance_width;
        i32 left_side_bearing;
        stbtt_GetCodepointHMetrics( &info, c, &advance_width, &left_side_bearing );

        width += ( i32 )roundf( advance_width * scale );
    }

    return width;
}

void FontAssetLoader::init( Allocator* allocator_, u32 size, AssetManager* asset_manager, GpuDevice* gpu_device_ ) {
    AssetLoader<FontAsset>::init( allocator_, size, asset_manager );

    gpu_device = gpu_device_;
    allocator = allocator_;
}

void FontAssetLoader::shutdown() {
    AssetLoader<FontAsset>::shutdown();
}

FontAsset* FontAssetLoader::load( StringView path ) {
    const u64 hashed_path = hash_calculate( path );
    FontAsset* texture = path_to_asset.get( hashed_path );

    if ( texture ) {
        texture->reference_count++;
        return texture;
    }

    texture = assets.obtain();
    iassert( texture );

    texture->reference_count = 1;

    // Actually load the font from the ttf file and create a texture
    Span<char> font_file = file_read_allocate( path, allocator );

    if ( !font_file.data ) {
        ilog_error( "Failed loading font %s\n", path );
        return nullptr;
    }

    stbtt_fontinfo font_info;
    if ( !stbtt_InitFont( &font_info, ( u8* )font_file.data, 0 ) ) {
        ilog_error( "Failed loading font %s\n", path );

        return nullptr;
    }

    // TODO:
    int line_height = 16;
    // Cache per character data and create bitmap
    int guessed_width = calculate_bitmap_width( font_info, line_height );
    int bitmap_width = round_up_to_power_of_2( guessed_width );
    int bitmap_height = round_up_to_power_of_2( line_height );
    u8* bitmap = iallocm( bitmap_width * bitmap_height, allocator );
    memset( bitmap, 0, bitmap_width * bitmap_height );

    // calculate font scaling
    const f32 scale = stbtt_ScaleForPixelHeight( &font_info, line_height * 1.0f );

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics( &font_info, &ascent, &descent, &line_gap );

    ascent = ( i32 )roundf( ascent * scale );
    descent = ( i32 )roundf( descent * scale );

    int x = 0;

    FontInfo& font_data = texture->info;

    for ( char c = FontInfo::k_first_char; c < FontInfo::k_last_char; ++c ) {
        font_data.char_start_x[ c - FontInfo::k_first_char ] = x;

        int advance_width;
        int left_side_bearing;
        stbtt_GetCodepointHMetrics( &font_info, c, &advance_width, &left_side_bearing );
        left_side_bearing = ( i32 )roundf( left_side_bearing * scale );

        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox( &font_info, c, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2 );

        // compute y (different characters have different heights)
        int y = ascent + c_y1;

        // render character (stride and offset is important here)
        int byteOffset = x + left_side_bearing + ( y * bitmap_width );
        stbtt_MakeCodepointBitmap( &font_info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, bitmap_width, scale, scale, c );

        // advance x
        x += ( i32 )roundf( advance_width * scale );
    }

    font_data.char_start_x[ FontInfo::k_num_chars ] = x;

    ifree( font_file.data, allocator );

    font_data.texture_width = bitmap_width;
    font_data.texture_height = bitmap_height;
    font_data.line_height = line_height;

    // Create texture
    sizet num_pixels = font_data.texture_width * font_data.texture_height;
    texture->rgba_bitmap_memory = iallocm( num_pixels * 4, allocator );
    iassert( texture->rgba_bitmap_memory );
    for ( size_t i = 0; i < num_pixels; ++i ) {
        u8* out_pixel = &texture->rgba_bitmap_memory[ i * 4 ];
        out_pixel[ 0 ] = bitmap[ i ];
        out_pixel[ 1 ] = bitmap[ i ];
        out_pixel[ 2 ] = bitmap[ i ];
        out_pixel[ 3 ] = bitmap[ i ];
    }


    texture->texture = gpu_device->create_texture( { .width = ( u16 )font_data.texture_width, .height = ( u16 )font_data.texture_height, .depth = 1, .array_layer_count = 1,
                                          .mip_level_count = 1, .flags = TextureFlags::Default_mask,
                                          .format = TextureFormat::R8G8B8A8_UNORM, .type = TextureType::Texture2D,
                                          .initial_data = texture->rgba_bitmap_memory, .debug_name = "Font Texture" } );

    ifree( bitmap, allocator );

    texture->path = asset_manager->allocate_path( path );

    path_to_asset.insert( hashed_path, texture );

    return texture;
}

void FontAssetLoader::unload( StringView path ) {

    const u64 hashed_path = hash_calculate( path );
    FontAsset* asset = path_to_asset.get( hashed_path );

    unload( asset );
}

void FontAssetLoader::unload( FontAsset* asset ) {

    if ( asset ) {
        asset->reference_count--;

        if ( asset->reference_count == 0 ) {
            gpu_device->destroy_texture( asset->texture );

            if ( asset->rgba_bitmap_memory ) {
                ifree( asset->rgba_bitmap_memory, allocator );
            }

            const u64 hashed_path = hash_calculate( asset->path.path );
            path_to_asset.remove( hashed_path );
            assets.release( asset );
            asset_manager->free_path( asset->path );
        }
    }
}



} // namespace idra