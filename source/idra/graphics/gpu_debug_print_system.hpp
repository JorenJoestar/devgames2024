/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_resources.hpp"
#include "graphics/render_system_interface.hpp"

namespace idra {

struct AssetManager;
struct Camera;
struct CommandBuffer;
struct GpuDevice;
struct ShaderAsset;

struct GpuDebugPrintSystem : public RenderSystemInterface {

    enum RenderPhase {
        Dispatch = 0,
        Draw,
        Count
    };

    void                        init( GpuDevice* gpu_device, Allocator* resident_allocator ) override;
    void                        shutdown() override;

    void                        update( f32 delta_time ) {}
    void                        render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) override;

    void                        create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) override;
    void                        destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) override;

    GpuDevice*                  gpu_device;

    ShaderAsset*                dispatch_shader;
    PipelineHandle              dispatch_pso;
    DescriptorSetLayoutHandle   dispatch_dsl;
    DescriptorSetHandle         dispatch_ds;

    ShaderAsset*                draw_shader;
    PipelineHandle              draw_pso;
    DescriptorSetLayoutHandle   draw_dsl;
    DescriptorSetHandle         draw_ds;

    BufferHandle                constants_ub;
    BufferHandle                entries_ub;
    BufferHandle                dispatches_ub;
    BufferHandle                indirect_buffer;

    u32                         dynamic_draw_offset;

}; // struct GpuDebugPrintSystem

} // namespace idra
