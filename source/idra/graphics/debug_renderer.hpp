/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_resources.hpp"

#include "graphics/render_system_interface.hpp"

#include "cglm/struct/vec3.h"
#include "kernel/color.hpp"

namespace idra {

struct AssetManager;
struct Camera;
struct CommandBuffer;
struct GpuDevice;
struct ShaderAsset;

//
//
struct DebugRenderer : public RenderSystemInterface {

                            DebugRenderer( u32 view_count, u32 max_lines );

    void                    init( GpuDevice* gpu_device, Allocator* resident_allocator ) override;
    void                    shutdown() override;

    void                    update( f32 delta_time ) {}
    void                    render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) override;

    void                    create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) override;
    void                    destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) override;



    void                    line( const vec3s& from, const vec3s& to, Color color, u32 view_index );
    void                    line_2d( const vec2s& from, const vec2s& to, Color color, u32 view_index );
    void                    line( const vec3s& from, const vec3s& to, Color color0, Color color1, u32 view_index );

    void                    aabb( const vec3s& min, const vec3s max, Color color, u32 view_index );

    GpuDevice*              gpu_device              = nullptr;

    // CPU rendering resources
    BufferHandle            lines_vb;
    BufferHandle            lines_vb_2d;

    u32                     view_count              = 0;
    u32                     max_lines               = 0;

    Array<u32>              current_line_per_view;
    Array<u32>              current_line_2d_per_view;

    // Shared resources
    PipelineHandle          debug_lines_draw_pipeline;
    PipelineHandle          debug_lines_2d_draw_pipeline;
    DescriptorSetLayoutHandle debug_lines_layout;
    DescriptorSetHandle     debug_lines_draw_set;

    ShaderAsset*            draw_shader     = nullptr;
    ShaderAsset*            draw_2d_shader  = nullptr;

}; // struct DebugRenderer


} // namespace crb