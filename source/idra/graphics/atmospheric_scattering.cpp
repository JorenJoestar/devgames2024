/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "graphics/atmospheric_scattering.hpp"

#include "graphics/graphics_asset_loaders.hpp"

#include "gpu/command_buffer.hpp"

#include "kernel/camera.hpp"
#include "kernel/numerics.hpp"

#include "imgui/imgui_helpers.hpp"

#include "external/cglm/struct/affine.h"

// Include glsl file to get the atmosphere struct
#define vec3 vec3s
#define vec4 vec4s
#define mat4 mat4s
#define uint u32
#include "../data/shaders/atmospheric_scattering/definitions.glsl"
#undef vec3
#undef vec4
#undef mat4
#undef uint


static AtmosphereParameters         s_atmosphere_parameters;

static void setup_earth_atmosphere( AtmosphereParameters& info, f32 length_unit_in_meters );


namespace idra {

void AtmosphericScatteringRenderSystem::init( GpuDevice* gpu_device_, Allocator* resident_allocator ) {
    gpu_device = gpu_device_;
}

void AtmosphericScatteringRenderSystem::shutdown() {
}

void AtmosphericScatteringRenderSystem::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Startup ) {

        setup_earth_atmosphere( s_atmosphere_parameters, 1000.f );

        ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

        transmittance_lut_shader = shader_loader->compile_compute( {},
            { "platform.h", "atmospheric_scattering/definitions.glsl",
            "atmospheric_scattering/functions.glsl",
            "atmospheric_scattering/sky_common.h" }, 
            "atmospheric_scattering/transmittance_lut.comp", 
            "transmittance_lut");

        multiscattering_lut_shader = shader_loader->compile_compute( {},
            { "platform.h", "atmospheric_scattering/definitions.glsl",
            "atmospheric_scattering/functions.glsl",
            "atmospheric_scattering/sky_common.h" },
            "atmospheric_scattering/multi_scattering.comp",
            "multiscattering_lut" );

        aerial_perspective_shader = shader_loader->compile_compute( {"MULTISCATAPPROX_ENABLED"},
            { "platform.h", "atmospheric_scattering/definitions.glsl",
            "atmospheric_scattering/functions.glsl",
            "atmospheric_scattering/sky_common.h" },
            "atmospheric_scattering/aerial_perspective.comp",
            "aerial_perspective" );

        sky_lut_shader = shader_loader->compile_compute( {"MULTISCATAPPROX_ENABLED"},
            { "platform.h", "atmospheric_scattering/definitions.glsl",
            "atmospheric_scattering/functions.glsl",
            "atmospheric_scattering/sky_common.h" },
            "atmospheric_scattering/sky_lut.comp",
            "sky_lut" );

        sky_apply_shader = shader_loader->compile_graphics( 
            { "MULTISCATAPPROX_ENABLED" },
            { "platform.h", "atmospheric_scattering/definitions.glsl",
            "atmospheric_scattering/functions.glsl",
            "atmospheric_scattering/sky_common.h" },
            "fullscreen_triangle.vert",
            "atmospheric_scattering/sky_apply.frag",
            "sky_apply" );


        sampler_clamp = gpu_device->create_sampler( {
            .min_filter = TextureFilter::Linear, .mag_filter = TextureFilter::Linear,
            .mip_filter = SamplerMipmapMode::Linear, .address_mode_u = SamplerAddressMode::Clamp_Border,
            .address_mode_v = SamplerAddressMode::Clamp_Border, .address_mode_w = SamplerAddressMode::Clamp_Border,
            .debug_name = "atmospheric scattering clamp sampler" } );


        transmittance_lut = gpu_device->create_texture( {
            .width = 256, .height = 64, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .sampler = sampler_clamp, .debug_name = "transmittance_lut" } );

        multiscattering_lut = gpu_device->create_texture( {
            .width = 32, .height = 32, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .debug_name = "multi_scattering_lut" } );

        sky_view_lut = gpu_device->create_texture( {
            .width = 192, .height = 108, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R11G11B10_FLOAT, .type = TextureType::Texture2D,
            .sampler = sampler_clamp, .debug_name = "sky_view_lut" } );

        aerial_perspective_texture = gpu_device->create_texture( {
            .width = 32, .height = 32, .depth = 32, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture3D,
            .debug_name = "aerial_perspective_texture" } );

        aerial_perspective_texture_debug = gpu_device->create_texture( {
            .width = 32, .height = 32, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .debug_name = "aerial_perspective_texture_debug" } );

        shared_dsl = gpu_device->create_descriptor_set_layout( {
            .dynamic_buffer_bindings = { 0 },
            .debug_name = "transmittance_lut_dsl" } );

        shared_ds = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( AtmosphereParameters )}},
            .layout = shared_dsl,
            .debug_name = "transmittance_lut_ds" } );
    }    

    // Update dependent assets/resources
    // NOTE: shaders are already reloaded, and just the shader handle is modified.
    // Just need to create the pipeline.
    transmittance_lut_pso = gpu_device->create_compute_pipeline( {
        .shader = transmittance_lut_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "transmittance_lut_pso" } );

    multiscattering_lut_pso = gpu_device->create_compute_pipeline( {
        .shader = multiscattering_lut_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "transmittance_lut_pso" } );

    aerial_perspective_pso = gpu_device->create_compute_pipeline( {
        .shader = aerial_perspective_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "aerial_perspective_pso" } );

    sky_lut_pso = gpu_device->create_compute_pipeline( {
        .shader = sky_lut_shader->shader,
        .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "sky_lut_pso" } );

    sky_apply_pso = gpu_device->create_graphics_pipeline( {
            .rasterization = {},
            .depth_stencil = {},
            .blend_state = {.blend_states = {{.source_color = Blend::SrcAlpha,
                                              .destination_color = Blend::InvSrcAlpha,
                                              .color_operation = BlendOperation::Add,
                                               } } },
            .vertex_input = {},
            .shader = sky_apply_shader->shader,
            .descriptor_set_layouts = { gpu_device->bindless_descriptor_set_layout, shared_dsl },
            .viewport = {},
            .color_formats = { gpu_device->swapchain_format },
            .depth_format = TextureFormat::D32_FLOAT,
            .debug_name = "sky_apply_pso" } );
}

void AtmosphericScatteringRenderSystem::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    // Destroy only the psos and return.
    gpu_device->destroy_pipeline( transmittance_lut_pso );
    gpu_device->destroy_pipeline( multiscattering_lut_pso );
    gpu_device->destroy_pipeline( aerial_perspective_pso );
    gpu_device->destroy_pipeline( sky_lut_pso );
    gpu_device->destroy_pipeline( sky_apply_pso );

    if ( phase == AssetDestructionPhase::Reload ) {
        return;
    }

    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

    shader_loader->unload( transmittance_lut_shader );
    shader_loader->unload( multiscattering_lut_shader );
    shader_loader->unload( aerial_perspective_shader );
    shader_loader->unload( sky_lut_shader );
    shader_loader->unload( sky_apply_shader );
    
    gpu_device->destroy_sampler( sampler_clamp );
    gpu_device->destroy_texture( transmittance_lut );
    gpu_device->destroy_texture( multiscattering_lut );
    gpu_device->destroy_texture( aerial_perspective_texture );
    gpu_device->destroy_texture( aerial_perspective_texture_debug );
    gpu_device->destroy_texture( sky_view_lut );
    gpu_device->destroy_descriptor_set_layout( shared_dsl );
    gpu_device->destroy_descriptor_set( shared_ds );
}

void AtmosphericScatteringRenderSystem::debug_ui() {

    if ( ImGui::Begin( "Atmospheric Scattering" ) ) {
        ImVec2 rt_size = ImGui::GetContentRegionAvail();
        ImGui::SliderUint( "Aerial Perspective Debug Slice", &aerial_perspective_debug_slice, 0, 31 );
        ImGui::Image( transmittance_lut, { 256, 64 } );
        ImGui::Image( multiscattering_lut, { 32 * 3, 32 * 3 } );
        ImGui::Image( aerial_perspective_texture_debug, { 256, 256 } );
        ImGui::Image( sky_view_lut, { 192 * 2, 108 * 2 } );
    }
    ImGui::End();
}

void AtmosphericScatteringRenderSystem::render( CommandBuffer* cb, Camera* camera, u32 phase ) {

    u32 constants_offset = 0;
    AtmosphereParameters* atmosphere_params = gpu_device->dynamic_buffer_allocate<AtmosphereParameters>( &constants_offset );
    if ( atmosphere_params ) {
        memcpy( atmosphere_params, &s_atmosphere_parameters, sizeof( AtmosphereParameters ) );

        atmosphere_params->inverse_view_projection = glms_mat4_inv( camera->view_projection );
        atmosphere_params->inverse_projection = glms_mat4_inv( camera->projection );
        atmosphere_params->inverse_view = glms_mat4_inv( camera->view );
        atmosphere_params->camera_position = camera->position;// scaling breaks a lot of things glms_vec3_scale( camera->position, 1.001f );

        const mat4s scale_matrix = glms_scale_make( { 1.f, -1.f, 1.f } );
        vec3s left_handed_sun_direction = glms_mat4_mulv3( scale_matrix, sun_direction, true );
        atmosphere_params->sun_direction = left_handed_sun_direction;
        atmosphere_params->mie_absorption = glms_vec3_maxv( glms_vec3_zero(), glms_vec3_sub( s_atmosphere_parameters.mie_extinction, s_atmosphere_parameters.mie_scattering ) );

        atmosphere_params->transmittance_lut_texture_index = transmittance_lut.index;
        atmosphere_params->aerial_perspective_texture_index = aerial_perspective_texture.index;
        atmosphere_params->aerial_perspective_debug_texture_index = aerial_perspective_texture_debug.index;
        atmosphere_params->aerial_perspective_debug_slice = aerial_perspective_debug_slice;
        atmosphere_params->sky_view_lut_texture_index = sky_view_lut.index;
        atmosphere_params->multiscattering_texture_index = multiscattering_lut.index;
        atmosphere_params->scene_color_texture_index = scene_color.index;
        atmosphere_params->scene_depth_texture_index = scene_depth.index;
    }


    if ( phase == CalculateLuts ) {

        cb->push_marker( "atmospheric scattering" );

        // Transmittance //////////////////////////////////////////////////////
        cb->push_marker( "transmittance lut" );
        cb->submit_barriers( { {transmittance_lut, ResourceState::UnorderedAccess, 0, 1} },
                             {  } );
        cb->bind_pipeline( transmittance_lut_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { constants_offset } );
        cb->dispatch_2d( 256, 64, 32, 32 );

        cb->submit_barriers( { {transmittance_lut, ResourceState::ShaderResource, 0, 1} }, {} );
        cb->pop_marker();

        // Multi-scattering ///////////////////////////////////////////////////
        cb->push_marker( "multiscattering lut" );
        cb->submit_barriers( { {multiscattering_lut, ResourceState::UnorderedAccess, 0, 1} },
                             {  } );
        cb->bind_pipeline( multiscattering_lut_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { constants_offset } );
        cb->dispatch_2d( 32, 32, 1, 1 );

        cb->submit_barriers( { {multiscattering_lut, ResourceState::ShaderResource, 0, 1} }, {} );

        cb->pop_marker();

        // Aerial perspective /////////////////////////////////////////////////
        cb->push_marker( "aerial perspective" );
        cb->submit_barriers( { {aerial_perspective_texture, ResourceState::UnorderedAccess, 0, 1},
                             {aerial_perspective_texture_debug, ResourceState::UnorderedAccess, 0, 1} },
                             {  } );
        cb->bind_pipeline( aerial_perspective_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { constants_offset } );
        cb->dispatch_3d( 32, 32, 32, 8, 8, 1 );

        cb->submit_barriers( { {aerial_perspective_texture, ResourceState::ShaderResource, 0, 1},
                             {aerial_perspective_texture_debug, ResourceState::UnorderedAccess, 0, 1} }, {} );
        cb->pop_marker();

        // Sky view ///////////////////////////////////////////////////////////
        cb->push_marker( "sky view" );
        cb->submit_barriers( { {sky_view_lut, ResourceState::UnorderedAccess, 0, 1} },
                             {  } );
        cb->bind_pipeline( sky_lut_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { constants_offset } );

        cb->dispatch_2d( 192, 108, 32, 32 );

        cb->submit_barriers( { {sky_view_lut, ResourceState::ShaderResource, 0, 1} }, {} );
        cb->pop_marker();

        cb->pop_marker();
    }
    else if ( phase == ApplyScattering ) {

        // Scene composition //////////////////////////////////////////////////
        cb->push_marker( "sky apply" );
        cb->submit_barriers( { {scene_color, ResourceState::RenderTarget, 0, 1},
                             {scene_depth, ResourceState::RenderTarget, 0, 1} },
                             {  } );

        cb->begin_pass( { scene_color }, { LoadOperation::Load }, { {0,0,0,0} }, scene_depth, LoadOperation::Load, {} );
        cb->set_framebuffer_scissor();
        cb->set_framebuffer_viewport();

        cb->bind_pipeline( sky_apply_pso );
        cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { constants_offset } );
        cb->draw( TopologyType::Triangle, 0, 3, 0, 1 );

        cb->end_render_pass();

        cb->submit_barriers( { {scene_color, ResourceState::ShaderResource, 0, 1},
                             { scene_depth, ResourceState::ShaderResource, 0, 1 } }, {} );
        cb->pop_marker();
    }    
}

} // namespace idra



//
// Setup earth atmosphere parameters. Copied from Sky UE4 demo.
//
void setup_earth_atmosphere( AtmosphereParameters& info, f32 length_unit_in_meters ) {
    // Values shown here are the result of integration over wavelength power spectrum integrated with paricular function.
    // Refer to https://github.com/ebruneton/precomputed_atmospheric_scattering for details.

    // All units in kilometers
    const float EarthBottomRadius = 6360000.0f / length_unit_in_meters;
    const float EarthTopRadius = 6460000.0f / length_unit_in_meters;   // 100km atmosphere radius, less edge visible and it contain 99.99% of the atmosphere medium https://en.wikipedia.org/wiki/K%C3%A1rm%C3%A1n_line
    const float EarthRayleighScaleHeight = 8.0f;
    const float EarthMieScaleHeight = 1.2f;

    // Sun - This should not be part of the sky model...
    //info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };
    info.solar_irradiance = { 1.0f, 1.0f, 1.0f };	// Using a normalise sun illuminance. This is to make sure the LUTs acts as a transfert factor to apply the runtime computed sun irradiance over.
    info.sun_angular_radius = 0.004675f;

    // Earth
    info.bottom_radius = EarthBottomRadius;
    info.top_radius = EarthTopRadius;
    info.ground_albedo = { 0.0f, 0.0f, 0.0f };

    // Move to more GPU friendly vec4 array.
    // Raleigh scattering
    //info.rayleigh_density.layers[ 0 ] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    info.rayleigh_density[ 0 ] = { 0, 0, 0, 0 };
    info.rayleigh_density[ 1 ] = { 0, 0, 1, -1.0f / EarthRayleighScaleHeight };
    info.rayleigh_density[ 2 ] = { 0, 0, -0.00142f, -0.00142f };
    //info.rayleigh_density.layers[ 1 ] = { 0.0f, 1.0f, -1.0f / EarthRayleighScaleHeight, 0.0f, 0.0f };
    info.rayleigh_scattering = { 0.005802f, 0.013558f, 0.033100f };		// 1/km

    // Mie scattering
    //info.mie_density.layers[ 0 ] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    //info.mie_density.layers[ 1 ] = { 0.0f, 1.0f, -1.0f / EarthMieScaleHeight, 0.0f, 0.0f };
    info.mie_density[ 0 ] = { 0, 0, 0, 0 };
    info.mie_density[ 1 ] = { 0, 0.0f, 1.0f, -1.0f / EarthMieScaleHeight };
    info.mie_density[ 2 ] = { 0, 0, -0.00142f, -0.00142f };
    info.mie_scattering = { 0.003996f, 0.003996f, 0.003996f };			// 1/km
    info.mie_extinction = { 0.004440f, 0.004440f, 0.004440f };			// 1/km
    info.mie_phase_function_g = 0.8f;

    // Ozone absorption
    //info.absorption_density.layers[ 0 ] = { 25.0f, 0.0f, 0.0f, 1.0f / 15.0f, -2.0f / 3.0f };
    info.absorption_density[ 0 ] = { 25.0f, 0.0f, 0.0f, 1.0f / 15.0f };
    info.absorption_density[ 1 ] = { -2.0f / 3.0f, 0, 0, 0 };
    info.absorption_density[ 2 ] = { -1.0f / 15.0f, 8.0f / 3.0f, -0.00142f, -0.00142f };
    //info.absorption_density.layers[ 1 ] = { 0.0f, 0.0f, 0.0f, -1.0f / 15.0f, 8.0f / 3.0f };
    info.absorption_extinction = { 0.000650f, 0.001881f, 0.000085f };	// 1/km

    const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
    info.mu_s_min = ( float )cos( max_sun_zenith_angle );
}

