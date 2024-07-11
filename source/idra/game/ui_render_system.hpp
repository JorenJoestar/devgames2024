/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "graphics/render_system_interface.hpp"

#include "cglm/struct/vec2.h"

namespace idra {

struct FontInfo;
struct FontAsset;
struct SpriteBatch;
struct TextureAsset;
struct UIBlueprint;

//
//
struct UIRenderSystem : public RenderSystemInterface {

    void                    init( GpuDevice* gpu_device, Allocator* resident_allocator ) override;
    void                    shutdown() override;

    void                    update( f32 delta_time ) {}
    void                    render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) {};

    void                    create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) override;
    void                    destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) override;

    f32                     font_get_height( const FontInfo& font_data );

    void                    add_box( SpriteBatch& sprite_batch, const vec2s& position, const vec2s& size );
    void                    add_text( SpriteBatch& sprite_batch, cstring text, const vec2s& position, bool screen_space );

    FontAsset*              font        = nullptr;
    UIBlueprint*            ui_blueprint = nullptr;
    TextureAsset*           ui_texture  = nullptr;

    GpuDevice*              gpu_device = nullptr;

    f32                     font_global_scale = 1.f;

}; // struct UIRenderSystem    

} // namespace idra