/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_resources.hpp"
#include "graphics/render_system_interface.hpp"

#include "cglm/struct/vec3.h"

namespace idra {

struct AssetManager;
struct Camera;
struct CommandBuffer;
struct GpuDevice;
struct ShaderAsset;

struct AtmosphericScatteringRenderSystem : public RenderSystemInterface {

    enum RenderPhase {
        CalculateLuts = 0,
        ApplyScattering,
        Count
    };

    void                        init( GpuDevice* gpu_device, Allocator* resident_allocator ) override;
    void                        shutdown() override;

    void                        update( f32 delta_time ) {}
    void                        render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) override;

    void                        create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) override;
    void                        destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) override;

    void                        debug_ui();

    GpuDevice*                  gpu_device;

    ShaderAsset*                transmittance_lut_shader;
    PipelineHandle              transmittance_lut_pso;

    ShaderAsset*                multiscattering_lut_shader;
    PipelineHandle              multiscattering_lut_pso;

    ShaderAsset*                aerial_perspective_shader;
    PipelineHandle              aerial_perspective_pso;

    ShaderAsset*                sky_lut_shader;
    PipelineHandle              sky_lut_pso;

    ShaderAsset*                sky_apply_shader;
    PipelineHandle              sky_apply_pso;

    // Shared
    DescriptorSetLayoutHandle   shared_dsl;
    DescriptorSetHandle         shared_ds;

    SamplerHandle               sampler_clamp;
 
    // Textures
    TextureHandle               transmittance_lut;
    TextureHandle               multiscattering_lut;
    TextureHandle               sky_view_lut;
    TextureHandle               aerial_perspective_texture;
    TextureHandle               aerial_perspective_texture_debug;

    // TODO: external dependency
    vec3s                       sun_direction;
    u32                         aerial_perspective_debug_slice = 16;
    TextureHandle               scene_color;
    TextureHandle               scene_depth;

}; // struct GpuDebugPrintSystem

} // namespace idra
