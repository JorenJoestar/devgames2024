/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "game/ui_render_system.hpp"

#include "kernel/asset.hpp"
#include "kernel/blob.hpp"
#include "kernel/numerics.hpp"

#include "gpu/gpu_device.hpp"

#include "graphics/graphics_asset_loaders.hpp"
#include "graphics/graphics_blueprints.hpp"
#include "graphics/sprite_batch.hpp"

#include "cglm/struct/vec2.h"

namespace idra {


static SpriteGPUData ui_box_sprites[ UIBlueprint::TextFrameElements::Count ];


void UIRenderSystem::init( GpuDevice* gpu_device_, Allocator* resident_allocator ) {

    gpu_device = gpu_device_;
}

void UIRenderSystem::shutdown() {
}

void UIRenderSystem::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Reload ) {
        return;
    }

    font = asset_manager->get_loader<FontAssetLoader>()->load( "../data/fonts/PixelFont.ttf" );

    // TODO:
    Allocator* allocator = g_memory->get_resident_allocator();
    Span<char> ui_blueprint_file = file_read_allocate( "data/ui.bui", allocator );

    if ( !ui_blueprint_file.data ) {
        ilog_error( "Failed loading ui blueprint %s\n", "data/ui.bui" );
        return;
    }

    BlobReader blob_reader;
    // TODO: allocation when versions are different ?
    ui_blueprint = blob_reader.read<UIBlueprint>( nullptr, UIBlueprint::k_version, ui_blueprint_file, false );

    ui_texture = asset_manager->get_loader<TextureAssetLoader>()->load( ui_blueprint->texture_name.c_str() );

    // Cache sprite informations
    Texture* atlas_texture = gpu_device->textures.get_cold( ui_texture->texture );
    for ( u32 i = 0; i < UIBlueprint::TextFrameElements::Count; ++i ) {
        const UITextFrameEntry& entry = ui_blueprint->text_frame_elements[ i ];
        SpriteGPUData& sprite = ui_box_sprites[ i ];

        sprite.set_albedo_id( atlas_texture->handle.index );
        sprite.set_screen_space_flag( 1 );
        sprite.uv_offset = { entry.uv_offset_x, entry.uv_offset_y };
        sprite.uv_size = { entry.uv_width, entry.uv_height };
        sprite.size = { entry.uv_width * atlas_texture->width, entry.uv_height * atlas_texture->height };
        sprite.position = { 0, 0, 0, 1 };
    }
}

void UIRenderSystem::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    if ( phase == AssetDestructionPhase::Reload ) {
        return;
    }

    // TODO:
    Allocator* allocator = g_memory->get_resident_allocator();
    ifree( ui_blueprint, allocator );

    asset_manager->get_loader<FontAssetLoader>()->unload( font );
    asset_manager->get_loader<TextureAssetLoader>()->unload( ui_blueprint->texture_name.c_str() );
}


f32 UIRenderSystem::font_get_height( const FontInfo& font_data ) {
    //const f32 vertical_margin = 0;// font_height +vertical_margin;
    return font_data.line_height * font_global_scale;
}


void UIRenderSystem::add_box( SpriteBatch& sprite_batch, const vec2s& position, const vec2s& size ) {

    // TODO:
    //// Background panels
    //const i32 tile_x_count = idra::roundi32( size.x / 32.f );
    //const i32 tile_y_count = idra::roundi32( size.y / 32.f );

    //for ( i32 i = 0; i < tile_y_count; ++i ) {
    //    for ( i32 j = 0; j < tile_x_count; ++j ) {

    //        SpriteGPUData& s = ui_box_sprites[ UIBlueprint::TextFrameElements::background_panel ];
    //        s.position = { position.x + j * 32.f + 16.f, position.y + i * 32.f + 16.f, 0, 1.f };

    //        //sprite_batch.add( s );
    //    }
    //}

    static f32 tl_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::TopLeft ].position_offset_x;
    static f32 tl_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::TopLeft ].position_offset_y;

    // Top left corner
    SpriteGPUData& tl = ui_box_sprites[ UIBlueprint::TextFrameElements::TopLeft ];
    tl.position = { position.x + tl_offset_x, position.y + tl_offset_y, 0, 1.f };
    sprite_batch.add( tl );

    static f32 bl_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::BottomLeft ].position_offset_x;
    static f32 bl_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::BottomLeft ].position_offset_y;

    // Bottom left corner
    SpriteGPUData& bl = ui_box_sprites[ UIBlueprint::TextFrameElements::BottomLeft ];
    bl.position = { position.x + bl_offset_x, position.y + size.y + bl_offset_y, 0, 1.f };
    sprite_batch.add( bl );

    const u32 border_size = 8;

    static f32 ht_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Top ].position_offset_x;
    static f32 ht_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Top ].position_offset_y;
    static f32 hb_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Bottom ].position_offset_x;
    static f32 hb_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Bottom ].position_offset_y;

    // Horizontal borders
    i32 horizontal_border_count = idra::roundi32( size.x / border_size ) - 1;
    if ( horizontal_border_count > 0 ) {
        // Add horizontal top borders
        for ( i32 i = 0; i < horizontal_border_count; ++i ) {
            SpriteGPUData& s = ui_box_sprites[ UIBlueprint::TextFrameElements::Top ];
            s.position = { position.x + ( border_size * ( i + 1 ) ) + ht_offset_x, position.y + ht_offset_y, 0, 1.f };
            sprite_batch.add( s );
        }

        // Add horizontal bottom borders
        for ( i32 i = 0; i < horizontal_border_count; ++i ) {
            SpriteGPUData& s = ui_box_sprites[ UIBlueprint::TextFrameElements::Bottom ];
            s.position = { position.x + ( border_size * ( i + 1 ) ) + hb_offset_x, position.y + size.y + hb_offset_y, 0, 1.f };
            sprite_batch.add( s );
        }
    }

    static f32 vl_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Left ].position_offset_x;
    static f32 vl_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Left ].position_offset_y;
    static f32 vr_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Right ].position_offset_x;
    static f32 vr_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::Right ].position_offset_y;

    // Vertical borders
    i32 vertical_border_count = idra::roundi32( size.y / border_size ) - 1;
    if ( vertical_border_count > 0 ) {
        // Add vertical left borders
        for ( i32 i = 0; i < vertical_border_count; ++i ) {
            SpriteGPUData& s = ui_box_sprites[ UIBlueprint::TextFrameElements::Left ];
            s.position = { position.x + vl_offset_x, position.y + ( border_size * ( i + 1 ) ) + vl_offset_y, 0, 1.f };
            sprite_batch.add( s );
        }

        // Add vertical right borders
        for ( i32 i = 0; i < vertical_border_count; ++i ) {
            SpriteGPUData& s = ui_box_sprites[ UIBlueprint::TextFrameElements::Right ];
            s.position = { position.x + vr_offset_x + ( horizontal_border_count * border_size ), position.y + ( border_size * ( i + 1 ) ) + vr_offset_y, 0, 1.f };
            sprite_batch.add( s );
        }
    }


    static f32 tr_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::TopRight ].position_offset_x;
    static f32 tr_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::TopRight ].position_offset_y;

    // Top right
    SpriteGPUData& tr = ui_box_sprites[ UIBlueprint::TextFrameElements::TopRight ];
    tr.position = { position.x + ( horizontal_border_count * border_size ) + tr_offset_x, position.y + tr_offset_y, 0, 1.f };
    sprite_batch.add( tr );

    static f32 br_offset_x = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::BottomRight ].position_offset_x;
    static f32 br_offset_y = ui_blueprint->text_frame_elements[ UIBlueprint::TextFrameElements::BottomRight ].position_offset_y;

    // Bottom right
    SpriteGPUData& br = ui_box_sprites[ UIBlueprint::TextFrameElements::BottomRight ];
    br.position = { position.x + ( horizontal_border_count * border_size ) + br_offset_x, position.y + size.y + br_offset_y, 0, 1.f };
    sprite_batch.add( br );
}


void UIRenderSystem::add_text( SpriteBatch& sprite_batch, cstring text, const vec2s& position, bool screen_space ) {

    const FontInfo& font_info = font->info;// screen_space ? screen_font_info : world_font_info;

    SpriteGPUData s;
    s.position = { position.x, position.y, 0, 1.f };
    s.set_albedo_id( font->texture.index );

    const f32 pixel_font_height = font_get_height( font_info );
    s.size.y = pixel_font_height;

    const char* char_ptr = text;
    while ( *char_ptr ) {
        const char c = *char_ptr++;

        if ( ( c == '\n' ) || ( c == '\r' ) ) {
            s.position.x = position.x;
            s.position.y += pixel_font_height;
            continue;
        }

        const u16 start_x = font_info.char_start_x[ c - FontInfo::k_first_char ];
        const u16 next_start_x = font_info.char_start_x[ c + 1 - FontInfo::k_first_char ];
        s.size.x = ( f32 )( next_start_x - start_x ) * font_global_scale;
        // NOTE: empirically flip the UV, not sure why 0.5.
        s.uv_offset = { f32( start_x ) / font_info.texture_width, 0 };
        s.uv_size = { ( s.size.x / font_global_scale ) / font_info.texture_width, ( s.size.y / font_global_scale ) / font_info.texture_height };

        s.set_screen_space_flag( screen_space );

        sprite_batch.add( s );
        s.position.x += s.size.x;
    }
}

} // namespace idra