#pragma once

#include "kernel/array.hpp"

#include "gpu/gpu_resources.hpp"

#include "cglm/struct/vec2.h"
#include "cglm/struct/vec4.h"

namespace idra {

struct Buffer;
struct Camera;
struct CommandBuffer;
struct GpuDevice;
struct SpriteGPUData;
struct MaterialPass;


// NOTE: this is linked to the shader used.
//
//
struct SpriteGPUData {

    vec4s                           position;

    vec2s                           uv_size;
    vec2s                           uv_offset;

    vec2s                           size;
    u32                             flag0;
    u32                             flag1;

    void                            set_screen_space_flag( bool value ) { flag0 = value ? 1 : 0; }

    void                            set_albedo_id( u32 albedo_id )  { flag1 = albedo_id; }
    u32                             get_albedo_it() const           { return flag1; }

}; // struct SpriteGPUData

struct SpriteGPUConstants {
    mat4s                   view_projection_matrix;
    mat4s                   projection_matrix_2d;

    u32                     screen_width;
    u32                     screen_height;
    u32                     disable_non_uniform_ext;
    u32                     pad30;
}; // struct SpriteGPUConstants

//
//
struct DrawBatch {

    PipelineHandle                  pipeline;
    DescriptorSetHandle             resource_list;
    u32                             offset;
    u32                             count;
}; // struct DrawBatch

//
//
struct SpriteBatch {

    void                            init( GpuDevice* gpu_device, Allocator* allocator );
    void                            shutdown();

    // TODO: maybe remove this and make it implicit ?
    void                            begin();
    void                            end();

    void                            add( SpriteGPUData& data );
    void                            set( PipelineHandle pipeline, DescriptorSetHandle descriptor_set );

    void                            draw( CommandBuffer* cb, Camera* camera, u32 phase );

    Array<DrawBatch>                draw_batches;

    GpuDevice*                      gpu_device;
    BufferHandle                    sprite_instance_vb;

    SpriteGPUData*                  gpu_data        = nullptr;
    u32                             num_sprites     = 0;
    u32                             previous_offset = 0;

    PipelineHandle                  current_pipeline;
    DescriptorSetHandle             current_descriptor_set;
    u32                             dynamic_buffer_offset = 0;

}; // struct SpriteBatch

} // namespace idra
