/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/asset.hpp"
#include "kernel/string.hpp"
#include "kernel/array.hpp"

#include "gpu/gpu_resources.hpp"

namespace idra {

struct AtlasBlueprint;
struct GpuDevice;
struct SpriteAnimationBlueprint;
struct TextureBlueprint;

//
//
struct ShaderAssetCreation {

    cstring             defines[ 8 ];
    cstring             includes[ 8 ];

    u32                 num_defines = 0;
    u32                 num_includes = 0;

    StringView          source_path;
    StringView          destination_path;

    StringView          name;

    ShaderStage::Enum   stage;

}; // struct ShaderAssetCreation

//
//
struct ShaderAsset : public Asset {

    ShaderStateHandle   shader;

    // Reloading informations
    u32                 creation_index;
    u32                 creation_count;

}; // struct ShaderAsset

//
//
struct TextureAsset : public Asset {

    TextureHandle       texture;

#if defined ( IDRA_USE_COMPRESSED_TEXTURES )
    TextureBlueprint*   blueprint   = nullptr;
#else
    void*               texture_data = nullptr;
#endif // IDRA_USE_COMPRESSED_TEXTURES

}; // struct TextureAsset 

//
//
struct SpriteAnimationAsset : public Asset {

    SpriteAnimationBlueprint* blueprint;

}; // struct SpriteAnimationAsset

//
//
struct AtlasAsset : public Asset {

    AtlasBlueprint*     blueprint;  // The read-only part of an atlas
    TextureAsset*       texture;    // Dependant texture for this atlas

}; // struct AssetAtlas


//
//
struct FontInfo {
    static constexpr char   k_first_char = 32;
    static constexpr char   k_last_char = 127;
    static constexpr char   k_num_chars = k_last_char - k_first_char;

    u32                 texture_width;
    u32                 texture_height;
    u16                 line_height;
    u16                 char_start_x[ k_num_chars + 1 ];   // where each char starts in the texture. All chars are placed in order so
    // the width of the n-th char is given by (char_start_x[ N+1 ] - char_start_x[ N ])
}; // struct FontInfo

//
//
struct FontAsset : public Asset {

    FontInfo            info;
    TextureHandle       texture;
    u8*                 rgba_bitmap_memory;
}; // struct FontAsset


//
//
struct ShaderAssetLoader : public AssetLoader<ShaderAsset> {

    static constexpr u32 k_loader_index = 0;

    void                init( Allocator* allocator, u32 size, AssetManager* asset_manager, GpuDevice* gpu );
    void                shutdown() override;

    ShaderAsset*        compile_graphics( Span<const StringView> defines,
                                          Span<const StringView> includes,
                                          StringView vertex_path,
                                          StringView fragment_path,
                                          StringView name );

    ShaderAsset*        compile_compute( Span<const StringView> defines,
                                         Span<const StringView> includes,
                                         StringView path,
                                         StringView name );


    ShaderAsset*        load( StringView path );
    void                unload( StringView path );
    void                unload( ShaderAsset* shader );

    void                reload_assets();

    // Cache per shader creation info, returns index into array.
    u32                 cache_creation_info( Span<const StringView> defines,
                                             Span<const StringView> include_paths,
                                             StringView path, ShaderStage::Enum stage,
                                             StringView name );

    GpuDevice*          gpu_device = nullptr;

    StringArray         string_array;   // Used to cache strings and create views.
    Array<ShaderAssetCreation> shader_creations;

}; // struct ShaderAssetLoader

//
//
struct TextureAssetLoader : public AssetLoader<TextureAsset> {

    static constexpr u32 k_loader_index = 1;

    void                init( Allocator* allocator, u32 size, AssetManager* asset_manager, GpuDevice* gpu );
    void                shutdown() override;

    TextureAsset*       load( StringView path );
    void                unload( StringView path );
    void                unload( TextureAsset* texture );

    GpuDevice*          gpu_device = nullptr;

}; // struct TextureAssetLoader


//
//
struct TextureAtlasLoader : public AssetLoader<AtlasAsset> {

    static constexpr u32 k_loader_index = 2;

    void                init( Allocator* allocator, u32 size, AssetManager* asset_manager, GpuDevice* gpu );
    void                shutdown() override;

    AtlasAsset*         load( StringView path );
    void                unload( StringView path );
    void                unload( AtlasAsset* atlas );

    GpuDevice*          gpu_device  = nullptr;
    Allocator*          allocator   = nullptr;

}; // struct TextureAtlasLoader

//
//
struct SpriteAnimationAssetLoader : public AssetLoader<SpriteAnimationAsset> {

    static constexpr u32 k_loader_index = 3;

    void                init( Allocator* allocator, u32 size, AssetManager* asset_manager ) override;
    void                shutdown() override;

    SpriteAnimationAsset* load( StringView path );
    void                unload( StringView path );
    void                unload( SpriteAnimationAsset* asset );

    Allocator*          allocator;

}; // struct SpriteAnimationAssetLoader


//
//
struct FontAssetLoader : public AssetLoader<FontAsset> {

    static constexpr u32 k_loader_index = 4;

    void                init( Allocator* allocator, u32 size, AssetManager* asset_manager, GpuDevice* gpu_device );
    void                shutdown() override;

    FontAsset*          load( StringView path );
    void                unload( StringView path );
    void                unload( FontAsset* font );

    GpuDevice*          gpu_device;
    Allocator*          allocator;

}; // struct FontAssetLoader

} // namespace idra