/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_device.hpp"
#include "gpu/gpu_profiler.hpp"

#if defined (IDRA_VULKAN)

IDRA_VK_DEFINE_HANDLE( VkCommandBuffer )
struct VkImageMemoryBarrier2;
struct VkBufferMemoryBarrier2;

#endif // IDRA_VULKAN

namespace idra {

struct Pipeline;

//
//
struct CommandBuffer {

#if defined (IDRA_VULKAN)
    void                            init( GpuDevice* gpu );
    void                            shutdown();
#endif // IDRA_VULKAN

    //
    // Commands interface
    //

    //void                            begin();
    //void                            end();

    void                            begin_pass( Span<const TextureHandle> render_targets, Span<const LoadOperation::Enum> load_operations, 
                                                Span<const ClearColor> clear_values, TextureHandle depth, 
                                                LoadOperation::Enum depth_load_operation, ClearDepthStencil depth_stencil_clear );
    void                            end_render_pass();

    void                            bind_pipeline( PipelineHandle handle );
    void                            bind_vertex_buffer( BufferHandle handle, u32 binding, u32 offset );
    void                            bind_vertex_buffers( BufferHandle* handles, u32 first_binding, u32 binding_count, u32* offsets );
    void                            bind_index_buffer( BufferHandle handle, u32 offset, IndexType::Enum index_type );
    void                            bind_descriptor_set( Span<const DescriptorSetHandle> handles, Span<const u32> offsets );

    void                            set_framebuffer_viewport();
    void                            set_viewport( const Viewport& viewport );

    void                            set_framebuffer_scissor();
    void                            set_scissor( const Rect2DInt& rect );

    void                            push_constants( PipelineHandle pipeline, u32 offset, u32 size, void* data );

    void                            draw( TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count );
    void                            draw_indexed( TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance );
    void                            draw_indirect( BufferHandle handle, u32 draw_count, u32 offset, u32 stride );
    void                            draw_indirect_count( BufferHandle argument_buffer, u32 argument_offset, BufferHandle count_buffer, u32 count_offset, u32 max_draws, u32 stride );
    void                            draw_indexed_indirect( BufferHandle handle, u32 draw_count, u32 offset, u32 stride );

    void                            draw_mesh_task( u32 task_count );
    void                            draw_mesh_task_indirect( BufferHandle argument_buffer, u32 argument_offset, u32 command_count, u32 stride );
    void                            draw_mesh_task_indirect_count( BufferHandle argument_buffer, u32 argument_offset, BufferHandle count_buffer, u32 count_offset, u32 max_draws, u32 stride );

    void                            dispatch_1d( u32 total_threads_x, u32 workgroup_size_x );
    void                            dispatch_2d( u32 total_threads_x, u32 total_threads_y, u32 workgroup_size_x, u32 workgroup_size_y );
    void                            dispatch_3d( u32 total_threads_x, u32 total_threads_y, u32 total_threads_z, u32 workgroup_size_x, u32 workgroup_size_y, u32 workgroup_size_z );
    void                            dispatch_indirect( BufferHandle handle, u32 offset );

    void                            trace_rays( PipelineHandle pipeline, u32 width, u32 height, u32 depth );

    // Barriers
    void                            global_debug_barrier(); // Use only to debug barrier-related problems

    void                            submit_barriers( Span<const TextureBarrier> texture_barriers,
                                                     Span<const BufferBarrier> buffer_barriers );

    void                            clear_color_image( TextureHandle texture, ClearColor clear_color );
    void                            fill_buffer( BufferHandle buffer, u32 offset, u32 size, u32 data );

    void                            push_marker( StringView name );
    void                            pop_marker();

    // Non-drawing methods
    void                            upload_texture_data( TextureHandle texture, void* texture_data, BufferHandle staging_buffer, sizet staging_buffer_offset );
    void                            copy_texture( TextureHandle src, TextureHandle dst, ResourceState::Enum dst_state );
    void                            copy_texture( TextureHandle src, TextureSubResource src_sub, TextureHandle dst, TextureSubResource dst_sub, ResourceState::Enum dst_state );

    void                            copy_buffer( BufferHandle src, sizet src_offset, BufferHandle dst, sizet dst_offset, sizet size );

    void                            upload_buffer_data( BufferHandle buffer, void* buffer_data, BufferHandle staging_buffer, sizet staging_buffer_offset );
    void                            upload_buffer_data( BufferHandle src, BufferHandle dst );

    void                            reset();

    static const u32                k_depth_stencil_clear_index = k_max_image_outputs;

#if defined (IDRA_VULKAN)
    VkCommandBuffer                 vk_command_buffer;
    VkImageMemoryBarrier2*          vk_image_barriers   = nullptr;
    VkBufferMemoryBarrier2*         vk_buffer_barriers  = nullptr;
    u32                             num_vk_image_barriers = 0;
    u32                             num_vk_buffer_barriers = 0;

    VkQueryPool                     vk_time_query_pool;

    VulkanPipeline*                 current_pipeline;
#endif // IDRA_VULKAN

    GpuTimeQueryTree                time_query_tree;
    GpuDevice*                      gpu_device;
    QueueType::Enum                 type            = QueueType::Count;

    // Render sizes when inside a pass.
    u32                             frame_buffer_width = 0;
    u32                             frame_buffer_height = 0;
    bool                            is_recording;
    bool                            inside_pass;


}; // struct CommandBuffer


struct CommandBufferManager {

    void                    init( GpuDevice* gpu, u32 max_command_buffers );
    void                    shutdown();

    void                    free_unused_buffers( u32 current_frame );

    CommandBuffer*          get_graphics_command_buffer();
    CommandBuffer*          get_active_graphics_command_buffer( u32 index );
    CommandBuffer*          get_compute_command_buffer();
    CommandBuffer*          get_transfer_command_buffer();

    u32                     get_max_buffers_per_frame() const;

    Span<CommandBuffer>     get_command_buffer_span( u32 frame );

    Array<VkCommandPool>    vk_command_pools;
    Array<CommandBuffer>    command_buffers;
    Array<GPUTimeQuery>     time_queries;

    GpuDevice*              gpu_device;
    u32                     current_frame;
    u32                     queries_per_pool = 100;

    u8                      max_command_buffers_per_queue[ QueueType::Count ];
    u8                      used_command_buffers_per_queue[ QueueType::Count ];

}; // struct CommandBufferManager

} // namespace idra
