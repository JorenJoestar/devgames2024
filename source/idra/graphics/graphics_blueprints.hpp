/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/blob.hpp"
#include "kernel/file.hpp"

#include "gpu/gpu_resources.hpp"

#include "graphics/sprite_animation.hpp"

namespace idra {


// This define comes from the project.
// Needs to be defined also in shader compiler

#if defined ( IDRA_USE_COMPRESSED_TEXTURES )
//
//
struct TextureBlueprint : public Blob {

    static constexpr u32    k_version = 0;

    FileTime            source_last_write_time;
    sizet               source_last_size;

    TextureCreation     gpu_creation;
    RelativeString      name;
    RelativeArray<u8>   texture_data;

}; // struct TextureBlueprint

template<>
void BlobReader::serialize<TextureBlueprint>( TextureBlueprint* data );

#endif // IDRA_USE_COMPRESSED_TEXTURES

//
//
struct SpriteAnimationBlueprint : public Blob {

    static constexpr u32    k_version = 0;

    RelativeArray<SpriteAnimationCreation> animations;
}; // struct SpriteAnimationBlueprint


template<>
void BlobReader::serialize<SpriteAnimationCreation>( SpriteAnimationCreation* data );

template<>
void BlobReader::serialize<SpriteAnimationBlueprint>( SpriteAnimationBlueprint* data );


// Atlas blueprints ///////////////////////////////////////////////////////
struct AtlasEntry {
    f32                             uv_offset_x;
    f32                             uv_offset_y;
    f32                             uv_width;
    f32                             uv_height;
}; // struct AtlasEntry

struct AtlasBlueprint : public Blob {

    RelativeArray<AtlasEntry>       entries;
    RelativeArray<RelativeString>   entry_names;
    RelativeString                  texture_name;

    static constexpr u32            k_version = 0;

}; // struct AtlasBlueprint


template<>
void BlobReader::serialize<AtlasEntry>( AtlasEntry* data );

template<>
void BlobReader::serialize<AtlasBlueprint>( AtlasBlueprint* data );


// UI blueprint ///////////////////////////////////////////////////////////

//
//
struct UITextFrameEntry {

    f32                             uv_offset_x;
    f32                             uv_offset_y;
    f32                             uv_width;
    f32                             uv_height;

    f32                             position_offset_x;
    f32                             position_offset_y;

}; // struct UITextFrameEntry

//
//
struct UIBlueprint : public Blob {

    enum TextFrameElements {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Top,
        Right,
        Bottom,
        Left,
        Count
    };

    UITextFrameEntry                text_frame_elements[ TextFrameElements::Count ];
    RelativeArray<RelativeString>   entry_names;
    RelativeString                  texture_name;

    static constexpr u32            k_version = 0;

}; // struct UIBlueprint


template<>
void BlobReader::serialize<UITextFrameEntry>( UITextFrameEntry* data );

template<>
void BlobReader::serialize<UIBlueprint>( UIBlueprint* data );

} // namespace idra