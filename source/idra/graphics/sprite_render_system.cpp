#include "graphics/sprite_render_system.hpp"

#include "graphics/graphics_asset_loaders.hpp"
#include "graphics/graphics_blueprints.hpp"
#include "gpu/command_buffer.hpp"

namespace idra {

void SpriteRenderSystem::init( GpuDevice* gpu_device_, Allocator* resident_allocator ) {

    gpu_device = gpu_device_;

    sprite_batch.init( gpu_device, resident_allocator );
    sprites.init( resident_allocator, 32 );
}

void SpriteRenderSystem::shutdown() {

    sprites.shutdown();
    sprite_batch.shutdown();
}

void SpriteRenderSystem::update( f32 delta_time ) {

    // Update all sprites ?
}

void SpriteRenderSystem::render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) {
    
    sprite_batch.draw( gpu_commands, camera, phase );
}

void SpriteRenderSystem::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Startup ) {
        ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

        // Sprite data
        draw_shader = shader_loader->compile_graphics( {}, { "platform.h" },
                                                       "pixel_art.vert", "pixel_art.frag",
                                                       "sprite_shader" );

        draw_dsl = gpu_device->create_descriptor_set_layout( {
                .dynamic_buffer_bindings = { 0 },
                .debug_name = "sprite_layout" } );

        draw_ds = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( SpriteGPUConstants )}},
            .layout = draw_dsl,
            .debug_name = "sprite_ds" } );
    }

    // Update dependent assets/resources
    // NOTE: shaders are already reloaded, and just the shader handle is modified.
    // Just need to create the pipeline.
    draw_pso = gpu_device->create_graphics_pipeline( {
        .rasterization = {.cull_mode = CullMode::None },
        .depth_stencil = {.depth_comparison = ComparisonFunction::LessEqual,
                           .depth_enable = 1, .depth_write_enable = 1 },
        .blend_state = {},
        .vertex_input = {.vertex_streams = { {.binding = 0, .stride = 48, .input_rate = VertexInputRate::PerInstance} },
                         .vertex_attributes = { { 0, 0, 0, VertexComponentFormat::Float4 },
                                                { 1, 0, 16, VertexComponentFormat::Float4 },
                                                { 2, 0, 32, VertexComponentFormat::Float2 },
                                                { 3, 0, 40, VertexComponentFormat::Uint2 }}},
        .shader = draw_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, draw_dsl },
        .viewport = {},
        .color_formats = { gpu_device->swapchain_format },
        .depth_format = TextureFormat::D32_FLOAT,
        .debug_name = "sprite_pso" } );
}

void SpriteRenderSystem::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    gpu_device->destroy_pipeline( draw_pso );

    if ( phase == AssetDestructionPhase::Reload ) {
        return;
    }

    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

    shader_loader->unload( draw_shader );
    gpu_device->destroy_descriptor_set_layout( draw_dsl );
    gpu_device->destroy_descriptor_set( draw_ds );
}

Sprite* SpriteRenderSystem::create_sprite( StringView texture_path, AssetManager* asset_manager ) {

    Sprite* sprite = sprites.obtain();
    iassert( sprite );

    SpriteAnimationAssetLoader* animation_loader = asset_manager->get_loader<SpriteAnimationAssetLoader>();

    sprite->texture = asset_manager->get_loader<TextureAssetLoader>()->load( texture_path );

    sprite->active = true;
    sprite->sprite.position = { 0, 0, 0, -1 };
    sprite->sprite.uv_offset = { 0, 0 };
    sprite->sprite.uv_size = { 1, 1 };
    sprite->sprite.set_screen_space_flag( false );
    sprite->sprite.set_albedo_id( sprite->texture->texture.index );

    return sprite;
}

void SpriteRenderSystem::destroy_sprite( Sprite* animated_sprite, AssetManager* asset_manager ) {

    asset_manager->get_loader<TextureAssetLoader>()->unload( animated_sprite->texture );
    sprites.release( animated_sprite );
}

void SpriteRenderSystem::add_sprite_to_draw( Sprite* sprite ) {
    
    //// Collect sprites.
    // Set common material. Texture is the only thing changing,
    // but it is encoded in the sprite instance data.
    sprite_batch.set( draw_pso, draw_ds );
    sprite_batch.add( sprite->sprite );
}

void SpriteRenderSystem::add_sprite( f32 x, f32 y, f32 width, f32 height, TextureHandle albedo ) {

    SpriteGPUData gpu_sprite = { {x, y, 0,-1}, { 1.0f, 1.0f }, {0.0f, 0.0f}, { width, height }, 1, albedo.index };
    sprite_batch.add( gpu_sprite );
}

} // namespace idra