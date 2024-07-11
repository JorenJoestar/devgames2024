/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_resources.hpp"

#include "graphics/sprite_batch.hpp"
#include "graphics/sprite_animation.hpp"
#include "graphics/render_system_interface.hpp"

namespace idra {

struct Allocator;
struct AssetManager;
struct Camera;
struct CommandBuffer;
struct GpuDevice;
struct ShaderAsset;
struct TextureAsset;

//
//
struct Sprite {

    SpriteGPUData           sprite;
    TextureAsset*           texture;

    u32                     pool_index;
    bool                    active;
}; // struct AnimatedSprite

//
//
struct SpriteRenderSystem : public RenderSystemInterface {

    void                    init( GpuDevice* gpu_device, Allocator* resident_allocator ) override;
    void                    shutdown() override;

    void                    update( f32 delta_time ) override;
    void                    render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) override;

    void                    create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) override;
    void                    destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) override;

    Sprite*                 create_sprite( StringView texture_path, AssetManager* asset_manager );
    void                    destroy_sprite( Sprite* sprite, AssetManager* asset_manager );

    void                    add_sprite_to_draw( Sprite* sprite );

    void                    add_sprite( f32 x, f32 y, f32 width, f32 height, TextureHandle albedo );

    GpuDevice*              gpu_device = nullptr;

    SpriteBatch             sprite_batch;

    ShaderAsset*            draw_shader;
    PipelineHandle          draw_pso;
    DescriptorSetLayoutHandle draw_dsl;
    DescriptorSetHandle     draw_ds;

    ResourcePoolTyped<Sprite> sprites;

}; // struct SpriteRenderSystem

} // namespace idra
