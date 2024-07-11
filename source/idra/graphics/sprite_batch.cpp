#include "sprite_batch.hpp"

#include "cglm/struct/mat4.h"

#include "gpu/gpu_device.hpp"
#include "gpu/command_buffer.hpp"
#include "kernel/camera.hpp"

namespace idra {


// SpriteBatch ////////////////////////////////////////////////////////////
static const u32 k_max_sprites = 3000;

void SpriteBatch::init( GpuDevice* gpu_device_, Allocator* allocator ) {

    gpu_device = gpu_device_;

    draw_batches.init( allocator, 8 );

    sprite_instance_vb = gpu_device->create_buffer( {
        .type = BufferUsage::Vertex_mask, .usage = ResourceUsageType::Dynamic,
        .size = sizeof( SpriteGPUData ) * k_max_sprites, .persistent = 0, .device_only = 0, .initial_data = nullptr,
        .debug_name = "sprites_batch_vb" } );

    current_pipeline = { 0, 0 };
    current_descriptor_set = { 0, 0 };
}

void SpriteBatch::shutdown() {
    gpu_device->destroy_buffer( sprite_instance_vb );

    draw_batches.shutdown();
}

void SpriteBatch::begin() {

    // Map sprite instance data
    num_sprites = 0;
    gpu_data = ( SpriteGPUData* )gpu_device->map_buffer( sprite_instance_vb, 0, 0 );
}

void SpriteBatch::end() {
    
    set( { 0, 0 }, { 0, 0 } );

    gpu_device->unmap_buffer( sprite_instance_vb );
    gpu_data = nullptr;
}

void SpriteBatch::add( SpriteGPUData& data ) {

    if ( num_sprites == k_max_sprites ) {
        ilog_warn( "WARNING: sprite batch capacity finished. Increase it! Max sprites %u\n", k_max_sprites );
        return;
    }
    if ( num_sprites == 0 ) {
        begin();
    }
    gpu_data[ num_sprites++ ] = data;
}

void SpriteBatch::set( PipelineHandle pipeline, DescriptorSetHandle descriptor_set ) {
  
    const bool current_resources_valid = current_pipeline.is_valid() && current_descriptor_set.is_valid();
    const bool changed_resources = current_pipeline != pipeline || current_descriptor_set != descriptor_set;
    if ( current_resources_valid && changed_resources ) {
        // Add a batch
        DrawBatch batch { current_pipeline, current_descriptor_set, previous_offset, num_sprites - previous_offset };
        draw_batches.push( batch );
    }

    // Cache num sprites and current resources
    if ( changed_resources ) {
        previous_offset = num_sprites;

        current_pipeline = pipeline;
        current_descriptor_set = descriptor_set;
    }
}

void SpriteBatch::draw( CommandBuffer* cb, Camera* camera, u32 phase ) {

    if ( num_sprites == 0 ) {
        return;
    }

    end();

    // Allocate from dynamic buffer and cache the dynamic buffer offset
    SpriteGPUConstants* cb_data = gpu_device->dynamic_buffer_allocate<SpriteGPUConstants>( &dynamic_buffer_offset );
    // Copy constants
    if ( cb_data ) {
        // Calculate view projection matrix
        memcpy( cb_data->view_projection_matrix.raw, &camera->view_projection.m00, 64 );
        // Calculate 2D projection matrix
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
        memcpy( cb_data->projection_matrix_2d.raw, &ortho_projection[ 0 ][ 0 ], 64 );

        cb_data->screen_width = camera->viewport_width;
        cb_data->screen_height = camera->viewport_height;

        cb_data->disable_non_uniform_ext = 0;
    }

    const u32 batches_count = draw_batches.size;
    for ( u32 i = 0; i < batches_count; ++i ) {

        DrawBatch& batch = draw_batches[ i ];
        if ( batch.count ) {
            cb->bind_vertex_buffer( sprite_instance_vb, 0, 0 );
            cb->bind_pipeline( batch.pipeline );
            cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, batch.resource_list }, { dynamic_buffer_offset } );
            cb->draw( TopologyType::Triangle, 0, 6, batch.offset, batch.count);
        }
    }

    draw_batches.set_size( 0 );

    // Reset drawing
    num_sprites = 0;
    gpu_data = nullptr;
}

} // namespace idra
