/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "graphics/crt_post_process.hpp"

#include "kernel/camera.hpp"

#include "gpu/command_buffer.hpp"

#include "graphics/graphics_asset_loaders.hpp"
#include "graphics/graphics_blueprints.hpp"

#include "imgui/imgui.h"

namespace idra {


struct CRTPostMattiasLocals {
    vec4s   output_size;

    u32     frame_count = 0;
    f32     curvature = 0.0001f;
    f32     h_blur = .5f;
    f32     v_blur = 1.1f;

    f32     accumulation_modulation = 0.6f;
    f32     ghosting = 0.15f;
    f32     noise_amount = 0.015f;
    f32     flicker_amount = 0.f;

    f32     interferences = 0.004f;
    f32     scanroll = 0.f;
    f32     shadow_mask = 0.23f;
    f32     pad000;
};

struct CRTPostMattiasBlur {

    f32     h_blur = 1.0f;
    f32     v_blur = 1.0f;

    f32     pad000;
    f32     pad001;
};

static CRTPostMattiasLocals s_crt_mattias_constants;

void CRTPostprocess::init( GpuDevice* gpu_device_, Allocator* resident_allocator ) {

    gpu_device = gpu_device_;
}

void CRTPostprocess::shutdown() {
}

void CRTPostprocess::update( f32 delta_time ) {

}

void CRTPostprocess::render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) {

    auto get_shader_texture_size = []( u32 width, u32 height ) -> vec4s {
        return { width * 1.f, height * 1.f, 1.f / width, 1.f / height };
        };

    TextureHandle input = externals.input;
    TextureHandle output = externals.output;

    u32 constants_offset = 0;
    CRTPostMattiasLocals* gpu_constants = gpu_device->dynamic_buffer_allocate<CRTPostMattiasLocals>( &constants_offset );
    if ( gpu_constants ) {
        mem_copy( gpu_constants, &s_crt_mattias_constants, sizeof( CRTPostMattiasLocals ) );

        gpu_constants->frame_count = gpu_device->absolute_frame;
        gpu_constants->output_size = get_shader_texture_size( camera->viewport_width, camera->viewport_height );
    }

    u32 hblur_constants_offset = 0;
    CRTPostMattiasBlur* hblur_gpu_constants = gpu_device->dynamic_buffer_allocate<CRTPostMattiasBlur>( &hblur_constants_offset );
    if ( hblur_constants_offset ) {
        hblur_gpu_constants->h_blur = s_crt_mattias_constants.h_blur / camera->viewport_width;
        hblur_gpu_constants->v_blur = 0.f;
    }

    u32 vblur_constants_offset = 0;
    CRTPostMattiasBlur* vblur_gpu_constants = gpu_device->dynamic_buffer_allocate<CRTPostMattiasBlur>( &vblur_constants_offset );
    if ( vblur_constants_offset ) {
        vblur_gpu_constants->h_blur = 0.f;
        vblur_gpu_constants->v_blur = s_crt_mattias_constants.v_blur / camera->viewport_height;
    }

    // TODO: handle resize properly
    Texture* final_tex = gpu_device->textures.get_cold( output );
    if ( final_tex ) {

        Texture* accum_tex = gpu_device->textures.get_cold( newpixie_accumulation_texture );

        if ( accum_tex->width != final_tex->width || accum_tex->height != final_tex->height ) {
            gpu_device->resize_texture( newpixie_accumulation_texture, final_tex->width, final_tex->height );
            gpu_device->resize_texture( newpixie_previous_horizontal_blur_texture, final_tex->width, final_tex->height );
            gpu_device->resize_texture( newpixie_horizontal_blur_texture, final_tex->width, final_tex->height );
            gpu_device->resize_texture( newpixie_vertical_blur_texture, final_tex->width, final_tex->height );
        }
    }


    if ( type == Mattias_Singlepass ) {

        gpu_commands->push_marker( "CRT Post" );

        gpu_commands->submit_barriers( { {output, ResourceState::RenderTarget, 0, 1} }, {} );

        gpu_commands->begin_pass( { output }, { LoadOperation::Clear }, { {0,0,0,0} }, {}, LoadOperation::DontCare, {} );

        gpu_commands->bind_pipeline( mattias_singlepass_pso );
        gpu_commands->bind_descriptor_set( { gpu_device->bindless_descriptor_set, mattias_singlepass_ds }, { constants_offset } );
        gpu_commands->draw( TopologyType::Triangle, 0, 3, input.index, 1 );

        gpu_commands->end_render_pass();

        gpu_commands->submit_barriers( { {output, ResourceState::ShaderResource, 0, 1} }, {} );
        gpu_commands->pop_marker();
    } else if ( type == Newpixie_Multipass ) {

        gpu_commands->push_marker( "CRT Post" );

        // Accumulation
        {
            gpu_commands->submit_barriers( { {newpixie_accumulation_texture, ResourceState::RenderTarget, 0, 1} }, {} );

            gpu_commands->begin_pass( { newpixie_accumulation_texture }, { LoadOperation::Load }, { {0,0,0,0} }, {}, LoadOperation::DontCare, {} );

            gpu_commands->bind_pipeline( newpixie_accumulation_pass.pso );
            gpu_commands->bind_descriptor_set( { gpu_device->bindless_descriptor_set, newpixie_accumulation_pass.descriptor_set }, { constants_offset } );

            u32 texture_ids = ( ( input.index & 0xffff ) << 16 ) | ( ( newpixie_previous_horizontal_blur_texture.index & 0xffff ) );
            gpu_commands->draw( TopologyType::Triangle, 0, 3, texture_ids, 1 );

            gpu_commands->end_render_pass();

            gpu_commands->submit_barriers( { {newpixie_accumulation_texture, ResourceState::ShaderResource, 0, 1} }, {} );
        }

        // Horizontal blur
        {
            gpu_commands->submit_barriers( { {newpixie_horizontal_blur_texture, ResourceState::RenderTarget, 0, 1} }, {} );

            gpu_commands->begin_pass( { newpixie_horizontal_blur_texture }, { LoadOperation::Load }, { {0,0,0,0} }, {}, LoadOperation::DontCare, {} );

            gpu_commands->bind_pipeline( newpixie_blur_pass.pso );
            gpu_commands->bind_descriptor_set( { gpu_device->bindless_descriptor_set, newpixie_blur_pass.descriptor_set }, { hblur_constants_offset } );
            gpu_commands->draw( TopologyType::Triangle, 0, 3, newpixie_accumulation_texture.index, 1 );

            gpu_commands->end_render_pass();

            gpu_commands->submit_barriers( { {newpixie_horizontal_blur_texture, ResourceState::ShaderResource, 0, 1} }, {} );
        }

        // Copy horizontal blur
        {
            gpu_commands->copy_texture( newpixie_horizontal_blur_texture, newpixie_previous_horizontal_blur_texture, ResourceState::ShaderResource );
        }

        // Vertical blur
        {
            gpu_commands->submit_barriers( { {newpixie_vertical_blur_texture, ResourceState::RenderTarget, 0, 1} }, {} );

            gpu_commands->begin_pass( { newpixie_vertical_blur_texture }, { LoadOperation::Load }, { {0,0,0,0} }, {}, LoadOperation::DontCare, {} );

            gpu_commands->bind_pipeline( newpixie_blur_pass.pso );
            gpu_commands->bind_descriptor_set( { gpu_device->bindless_descriptor_set, newpixie_blur_pass.descriptor_set }, { vblur_constants_offset } );
            gpu_commands->draw( TopologyType::Triangle, 0, 3, newpixie_horizontal_blur_texture.index, 1 );

            gpu_commands->end_render_pass();

            gpu_commands->submit_barriers( { {newpixie_vertical_blur_texture, ResourceState::ShaderResource, 0, 1} }, {} );
        }

        // Final pass
        {
            gpu_commands->submit_barriers( { {output, ResourceState::RenderTarget, 0, 1} }, {} );

            gpu_commands->begin_pass( { output }, { LoadOperation::Clear }, { {0,0,0,0} }, {}, LoadOperation::DontCare, {} );

            gpu_commands->bind_pipeline( newpixie_main_pass.pso );
            gpu_commands->bind_descriptor_set( { gpu_device->bindless_descriptor_set, newpixie_main_pass.descriptor_set }, { constants_offset } );
            u32 texture_ids = ( ( newpixie_vertical_blur_texture.index & 0xffff ) << 16 ) | ( ( newpixie_accumulation_texture.index & 0xffff ) );
            gpu_commands->draw( TopologyType::Triangle, 0, 3, texture_ids, 1 );

            gpu_commands->end_render_pass();

            gpu_commands->submit_barriers( { {output, ResourceState::ShaderResource, 0, 1} }, {} );
        }

        gpu_commands->pop_marker();
    }
}

void CRTPostprocess::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Startup ) {
        ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

        // Single pass
        mattias_singlepass_shader = shader_loader->compile_graphics( {}, { "platform.h" },
                                                                     "fullscreen_triangle.vert", "mattias_crt/mattias_crt_singlepass.frag",
                                                                     "mattias_singlepass_shader" );

        mattias_singlepass_dsl = gpu_device->create_descriptor_set_layout( {
                .dynamic_buffer_bindings = { 0 },
                .debug_name = "mattias_singlepass_dsl" } );

        mattias_singlepass_ds = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( CRTPostMattiasLocals )}},
            .layout = mattias_singlepass_dsl,
            .debug_name = "mattias_singlepass_ds" } );

        // Accumulation
        newpixie_accumulation_pass.shader = shader_loader->compile_graphics( {}, { "platform.h" },
                                                                             "fullscreen_triangle.vert", "newpixie/accumulation.frag",
                                                                             "newpixie_accumulation_shader" );

        newpixie_accumulation_pass.descriptor_set_layout = gpu_device->create_descriptor_set_layout( {
                .dynamic_buffer_bindings = { 0 },
                .debug_name = "newpixie_accumulation_dsl" } );

        newpixie_accumulation_pass.descriptor_set = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( CRTPostMattiasLocals )}},
            .layout = newpixie_accumulation_pass.descriptor_set_layout,
            .debug_name = "newpixie_accumulation_ds" } );

        // Blur
        newpixie_blur_pass.shader = shader_loader->compile_graphics( {}, { "platform.h" },
                                                                     "fullscreen_triangle.vert", "newpixie/blur.frag",
                                                                     "newpixie_blur_shader" );

        newpixie_blur_pass.descriptor_set_layout = gpu_device->create_descriptor_set_layout( {
                .dynamic_buffer_bindings = { 0 },
                .debug_name = "newpixie_blur_dsl" } );

        newpixie_blur_pass.descriptor_set = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( CRTPostMattiasBlur )}},
            .layout = newpixie_blur_pass.descriptor_set_layout,
            .debug_name = "newpixie_blur_ds" } );

        // Multipass
        newpixie_main_pass.shader = shader_loader->compile_graphics( {}, { "platform.h" },
                                                                     "fullscreen_triangle.vert", "newpixie/multipass.frag",
                                                                     "newpixie_main_shader" );

        newpixie_main_pass.descriptor_set_layout = gpu_device->create_descriptor_set_layout( {
                .dynamic_buffer_bindings = { 0 },
                .debug_name = "newpixie_main_dsl" } );

        newpixie_main_pass.descriptor_set = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( CRTPostMattiasLocals )}},
            .layout = newpixie_main_pass.descriptor_set_layout,
            .debug_name = "newpixie_main_ds" } );


        newpixie_accumulation_texture = gpu_device->create_texture( {
            .width = ( u16 )gpu_device->swapchain_width, .height = ( u16 )gpu_device->swapchain_height, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask,
            .format = gpu_device->swapchain_format, .type = TextureType::Texture2D,
            .debug_name = "newpixie_accumulation_texture" } );

        newpixie_previous_horizontal_blur_texture = gpu_device->create_texture( {
            .width = ( u16 )gpu_device->swapchain_width, .height = ( u16 )gpu_device->swapchain_height, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask,
            .format = gpu_device->swapchain_format, .type = TextureType::Texture2D,
            .debug_name = "newpixie_previous_horizontal_blur_texture" } );

        newpixie_horizontal_blur_texture = gpu_device->create_texture( {
            .width = ( u16 )gpu_device->swapchain_width, .height = ( u16 )gpu_device->swapchain_height, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask,
            .format = gpu_device->swapchain_format, .type = TextureType::Texture2D,
            .debug_name = "newpixie_horizontal_blur_texture" } );

        newpixie_vertical_blur_texture = gpu_device->create_texture( {
            .width = ( u16 )gpu_device->swapchain_width, .height = ( u16 )gpu_device->swapchain_height, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask,
            .format = gpu_device->swapchain_format, .type = TextureType::Texture2D,
            .debug_name = "newpixie_vertical_blur_texture" } );
    }

    // Update dependent assets/resources
    // NOTE: shaders are already reloaded, and just the shader handle is modified.
    // Just need to create the pipeline.

    GraphicsPipelineCreation gpc = {
        .rasterization = {.cull_mode = CullMode::None },
        .depth_stencil = {.depth_comparison = ComparisonFunction::LessEqual,
                           .depth_enable = 1, .depth_write_enable = 1 },
        .blend_state = {},
        .vertex_input = {},
        .shader = mattias_singlepass_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, mattias_singlepass_dsl },
        .viewport = {},
        .color_formats = { gpu_device->swapchain_format },
        .debug_name = "mattias_singlepass_pso" };

    mattias_singlepass_pso = gpu_device->create_graphics_pipeline( gpc );

    gpc.debug_name = "newpixie_accumulation_pso";
    gpc.shader = newpixie_accumulation_pass.shader->shader;
    newpixie_accumulation_pass.pso = gpu_device->create_graphics_pipeline( gpc );

    gpc.debug_name = "newpixie_shader_pso";
    gpc.shader = newpixie_blur_pass.shader->shader;
    newpixie_blur_pass.pso = gpu_device->create_graphics_pipeline( gpc );

    gpc.debug_name = "newpixie_main_pso";
    gpc.shader = newpixie_main_pass.shader->shader;
    newpixie_main_pass.pso = gpu_device->create_graphics_pipeline( gpc );
}

void CRTPostprocess::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    gpu_device->destroy_pipeline( mattias_singlepass_pso );
    gpu_device->destroy_pipeline( newpixie_accumulation_pass.pso );
    gpu_device->destroy_pipeline( newpixie_blur_pass.pso );
    gpu_device->destroy_pipeline( newpixie_main_pass.pso );

    if ( phase == AssetDestructionPhase::Reload ) {
        return;
    }

    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

    shader_loader->unload( mattias_singlepass_shader );
    gpu_device->destroy_descriptor_set_layout( mattias_singlepass_dsl );
    gpu_device->destroy_descriptor_set( mattias_singlepass_ds );

    shader_loader->unload( newpixie_accumulation_pass.shader );
    gpu_device->destroy_descriptor_set_layout( newpixie_accumulation_pass.descriptor_set_layout );
    gpu_device->destroy_descriptor_set( newpixie_accumulation_pass.descriptor_set );

    shader_loader->unload( newpixie_blur_pass.shader );
    gpu_device->destroy_descriptor_set_layout( newpixie_blur_pass.descriptor_set_layout );
    gpu_device->destroy_descriptor_set( newpixie_blur_pass.descriptor_set );

    shader_loader->unload( newpixie_main_pass.shader );
    gpu_device->destroy_descriptor_set_layout( newpixie_main_pass.descriptor_set_layout );
    gpu_device->destroy_descriptor_set( newpixie_main_pass.descriptor_set );

    gpu_device->destroy_texture( newpixie_accumulation_texture );
    gpu_device->destroy_texture( newpixie_horizontal_blur_texture );
    gpu_device->destroy_texture( newpixie_previous_horizontal_blur_texture );
    gpu_device->destroy_texture( newpixie_vertical_blur_texture );
}

void CRTPostprocess::debug_ui() {

    if ( ImGui::Begin( "CRT" ) ) {
        cstring crt_types[]{ "None", "Lottes", "Mattias", "MattiasMulti" };
        ImGui::Combo( "CRT Type", &type, crt_types, ArraySize( crt_types ) );

        static i32 mask_type = 2;

        /*if ( crt_type == 1 ) {
            ImGui::SliderInt( "Mask", &mask_type, 0, 3 );
            s_crt_consts.MASK = mask_type;
            ImGui::SliderFloat( "Mask intensity", &s_crt_consts.MASK_INTENSITY, 0.f, 1.f );
            ImGui::SliderFloat( "Scanline thinness", &s_crt_consts.SCANLINE_THINNESS, 0.f, 1.f );
            ImGui::SliderFloat( "Scan blur", &s_crt_consts.SCAN_BLUR, 1.f, 3.f );
            ImGui::SliderFloat( "Curvature", &s_crt_consts.CURVATURE, 0.f, .25f );
            ImGui::SliderFloat( "Trinitron curve", &s_crt_consts.TRINITRON_CURVE, 0.f, 1.f );
            ImGui::SliderFloat( "Corner", &s_crt_consts.CORNER, 0.f, 11.f );
            ImGui::SliderFloat( "CRT gamma", &s_crt_consts.CRT_GAMMA, 0.f, 51.f );
        } else if ( crt_type == 2 || crt_type == 3 ) */
        {
            ImGui::SliderFloat( "Curvature", &s_crt_mattias_constants.curvature, 0.f, 0.25f );
            ImGui::SliderFloat( "Horizontal Blur", &s_crt_mattias_constants.h_blur, 0.f, 5.f );
            ImGui::SliderFloat( "Vertical Blur", &s_crt_mattias_constants.v_blur, 0.f, 5.f );

            ImGui::SliderFloat( "accumulation_modulation", &s_crt_mattias_constants.accumulation_modulation, 0.f, 1.f );
            ImGui::SliderFloat( "ghosting", &s_crt_mattias_constants.ghosting, 0.f, 1.f );
            ImGui::SliderFloat( "noise_amount", &s_crt_mattias_constants.noise_amount, 0.f, 1.f );
            ImGui::SliderFloat( "flicker_amount", &s_crt_mattias_constants.flicker_amount, 0.f, 1.f );
            ImGui::SliderFloat( "interferences", &s_crt_mattias_constants.interferences, 0.f, 1.f );
            ImGui::SliderFloat( "scanroll", &s_crt_mattias_constants.scanroll, 0.f, 1.f );
            ImGui::SliderFloat( "shadow_mask", &s_crt_mattias_constants.shadow_mask, 0.f, 1.f );
        }

        ImGui::Separator();

    }
    ImGui::End();
}

// GraphicsPostFullscreenPass /////////////////////////////////////////////

void GraphicsPostFullscreenPass::render( CommandBuffer* gpu_commands, Camera* camera ) {
}

void GraphicsPostFullscreenPass::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    //if ( phase == AssetCreationPhase::Startup ) {
    //    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

    //    // Single pass
    //    shader = shader_loader->compile_graphics( { "platform.h" },
    //                                                                 "fullscreen_triangle.vert", "mattias_crt/mattias_crt_singlepass.frag",
    //                                                                 "mattias_singlepass_shader" );

    //    descriptor_set_layout = gpu_device->create_descriptor_set_layout( {
    //            .dynamic_buffer_bindings = { 0 },
    //            .debug_name = "mattias_singlepass_dsl" } );

    //    descriptor_set = gpu_device->create_descriptor_set( {
    //        .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( CRTPostMattiasLocals )}},
    //        .layout = descriptor_set_layout,
    //        .debug_name = "mattias_singlepass_ds" } );
    //}

    //// Update dependent assets/resources
    //// NOTE: shaders are already reloaded, and just the shader handle is modified.
    //// Just need to create the pipeline.
    //pso = gpu_device->create_graphics_pipeline( {
    //    .rasterization = {.cull_mode = CullMode::None },
    //    .depth_stencil = {.depth_comparison = ComparisonFunction::LessEqual,
    //                       .depth_enable = 1, .depth_write_enable = 1 },
    //    .blend_state = {},
    //    .vertex_input = {},
    //    .shader = shader->shader,
    //    .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, descriptor_set_layout },
    //    .viewport = {},
    //    .color_formats = { gpu_device->swapchain_format },
    //    .debug_name = "mattias_singlepass_pso" } );
}

void GraphicsPostFullscreenPass::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {
}



} // namespace idra