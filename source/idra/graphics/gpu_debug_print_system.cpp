/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "graphics/gpu_debug_print_system.hpp"

#include "graphics/graphics_asset_loaders.hpp"
#include "gpu/command_buffer.hpp"

#include "kernel/camera.hpp"

namespace idra {

struct DebugGpuFontConstants {
    mat4s               view_projection_matrix;
    mat4s               projection_matrix_2d;

    u32                 screen_width;
    u32                 screen_height;
    u32                 padding0;
    u32                 padding1;
};

void GpuDebugPrintSystem::init( GpuDevice* gpu_device_, Allocator* resident_allocator ) {
    gpu_device = gpu_device_;
}

void GpuDebugPrintSystem::shutdown() {
}

void GpuDebugPrintSystem::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Startup ) {
        ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();
        dispatch_shader = shader_loader->compile_compute( {}, { "platform.h", "debug_print/debug_gpu_font.h" }, "debug_print/debug_gpu_text_dispatch.comp", "debug_gpu_text_dispatch" );

        dispatch_dsl = gpu_device->create_descriptor_set_layout( {
            .bindings = {
                {.type = DescriptorType::StructuredBuffer, .start = 2, .count = 1, .name = "src"},
                {.type = DescriptorType::StructuredBuffer, .start = 3, .count = 1, .name = "dst"},
                {.type = DescriptorType::StructuredBuffer, .start = 4, .count = 1, .name = "dst"},
                {.type = DescriptorType::StructuredBuffer, .start = 5, .count = 1, .name = "dst"},
            },
            .debug_name = "debug_gpu_text_dsl" } );

        // TODO: check sizes!
        constants_ub = gpu_device->create_buffer( {
            .type = BufferUsage::Structured_mask, .usage = ResourceUsageType::Dynamic,
            .size = 1024 * 16, .persistent = 0, .device_only = 0, .initial_data = nullptr,
            .debug_name = "gpu_font_ub" } );

        entries_ub = gpu_device->create_buffer( {
            .type = BufferUsage::Structured_mask, .usage = ResourceUsageType::Dynamic,
            .size = 1024 * 16, .persistent = 0, .device_only = 0, .initial_data = nullptr,
            .debug_name = "gpu_font_entries_ub" } );

        dispatches_ub = gpu_device->create_buffer( {
            .type = BufferUsage::Structured_mask, .usage = ResourceUsageType::Dynamic,
            .size = 1024 * 16, .persistent = 0, .device_only = 0, .initial_data = nullptr,
            .debug_name = "gpu_font_dispatches_ub" } );

        indirect_buffer = gpu_device->create_buffer( {
            .type = ( BufferUsage::Mask )( BufferUsage::Indirect_mask | BufferUsage::Structured_mask ), .usage = ResourceUsageType::Dynamic,
            .size = sizeof( f32 ) * 8, .persistent = 0, .device_only = 0, .initial_data = nullptr,
            .debug_name = "gpu_font_ub" } );

        dispatch_ds = gpu_device->create_descriptor_set( {
            .ssbos = {{constants_ub, 2}, {entries_ub, 3},
                      {dispatches_ub, 4},{indirect_buffer, 5}},
            .layout = dispatch_dsl,
            .debug_name = "debug_gpu_text_ds" } );

        draw_shader = shader_loader->compile_graphics( {}, { "platform.h", "debug_print/debug_gpu_font.h" },
                                                       "debug_print/debug_gpu_font.vert", "debug_print/debug_gpu_font.frag",
                                                       "debug_gpu_text_draw_shader" );

        draw_dsl = gpu_device->create_descriptor_set_layout( {
            .bindings = {
                {.type = DescriptorType::StructuredBuffer, .start = 2, .count = 1, .name = "src"},
                {.type = DescriptorType::StructuredBuffer, .start = 3, .count = 1, .name = "dst"},
                {.type = DescriptorType::StructuredBuffer, .start = 4, .count = 1, .name = "dst"},
                {.type = DescriptorType::StructuredBuffer, .start = 5, .count = 1, .name = "dst"},
            },
            .dynamic_buffer_bindings = { 0 }, .debug_name = "debug_gpu_text_draw_dsl" } );

        draw_ds = gpu_device->create_descriptor_set( {
            .ssbos = {{constants_ub, 2}, {entries_ub, 3},
                      {dispatches_ub, 4},{indirect_buffer, 5}},
                .dynamic_buffer_bindings = {{0, sizeof( DebugGpuFontConstants )}},
            .layout = draw_dsl,
            .debug_name = "debug_gpu_text_draw_ds" } );
    }    

    // Update dependent assets/resources
    // NOTE: shaders are already reloaded, and just the shader handle is modified.
    // Just need to create the pipeline.
    dispatch_pso = gpu_device->create_compute_pipeline( {
        .shader = dispatch_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, dispatch_dsl },
        .debug_name = "test_pso" } );

    draw_pso = gpu_device->create_graphics_pipeline( {
        .rasterization = {.cull_mode = CullMode::None },
        .depth_stencil = {.depth_comparison = ComparisonFunction::Always,
                           .depth_enable = 1, .depth_write_enable = 0 },
        .blend_state = {},
        .vertex_input = {.vertex_streams = { {.binding = 0, .stride = 16, .input_rate = VertexInputRate::PerInstance} },
                         .vertex_attributes = { { 0, 0, 0, VertexComponentFormat::Float4 }}},
        .shader = draw_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, draw_dsl },
        .viewport = {},
        .color_formats = { gpu_device->swapchain_format },
        .depth_format = TextureFormat::D32_FLOAT,
        .debug_name = "debug_gpu_text_dispatch_pso" } );

}

void GpuDebugPrintSystem::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    // Destroy only the psos and return.
    gpu_device->destroy_pipeline( dispatch_pso );
    gpu_device->destroy_pipeline( draw_pso );

    if ( phase == AssetDestructionPhase::Reload ) {
        return;
    }

    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();
    shader_loader->unload( dispatch_shader );
    shader_loader->unload( draw_shader );

    gpu_device->destroy_buffer( constants_ub );
    gpu_device->destroy_buffer( dispatches_ub );
    gpu_device->destroy_buffer( entries_ub );
    gpu_device->destroy_buffer( indirect_buffer );
    gpu_device->destroy_descriptor_set_layout( dispatch_dsl );
    gpu_device->destroy_descriptor_set_layout( draw_dsl );
    gpu_device->destroy_descriptor_set( dispatch_ds );
    gpu_device->destroy_descriptor_set( draw_ds );
}

void GpuDebugPrintSystem::render( CommandBuffer* cb, Camera* camera, u32 phase ) {

    // Dispatch phase
    if ( phase == Dispatch ) {

        DebugGpuFontConstants* gpu_data = gpu_device->dynamic_buffer_allocate<DebugGpuFontConstants>( &dynamic_draw_offset );
        if ( gpu_data ) {
            camera->get_projection_ortho_2d( gpu_data->projection_matrix_2d.raw );

            f32 L = 0;
            f32 R = camera->viewport_width * camera->zoom;
            f32 T = 0;
            f32 B = camera->viewport_height * camera->zoom;
            const f32 ortho_projection[ 4 ][ 4 ] =
            {
                { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
                { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
                { 0.0f,         0.0f,        -1.0f,   0.0f },
                { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
            };
            memcpy( gpu_data->projection_matrix_2d.raw, &ortho_projection[ 0 ][ 0 ], 64 );

            gpu_data->view_projection_matrix = camera->view_projection;
            gpu_data->screen_width = camera->viewport_width;
            gpu_data->screen_height = camera->viewport_height;
        }

        cb->push_marker( "debug_gpu_text_dispatch" );
        cb->submit_barriers( {},
                             { {constants_ub, ResourceState::ShaderResource},
                             {dispatches_ub, ResourceState::ShaderResource},
                             {entries_ub, ResourceState::ShaderResource},
                             {indirect_buffer, ResourceState::ShaderResource} } );
        cb->fill_buffer( constants_ub, 0, 64, 0 );
        cb->bind_pipeline( dispatch_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, dispatch_ds }, {} );
        cb->dispatch_1d( 1, 1 );

        cb->submit_barriers( {}, { {indirect_buffer, ResourceState::IndirectArgument} } );
        cb->pop_marker();
    }

    // Draw phase
    if ( phase == Draw ) {
        cb->bind_pipeline( draw_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, draw_ds }, { dynamic_draw_offset } );
        cb->draw_indirect( indirect_buffer, 1, 0, sizeof( u32 ) * 4 );
    }
}

} // namespace idra
