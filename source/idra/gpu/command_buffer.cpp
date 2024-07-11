#include "gpu/command_buffer.hpp"
#include "gpu/gpu_device.hpp"

#include "kernel/memory.hpp"
#include "kernel/numerics.hpp"

#include "gpu/gpu_device.hpp"

#if defined(_MSC_VER)
#include <windows.h>
#endif

namespace idra {

void CommandBuffer::reset() {

    is_recording = false;

    current_pipeline = nullptr;
    inside_pass = false;
    frame_buffer_width = 0;
    frame_buffer_height = 0;

    iassertm( num_vk_image_barriers == 0, "There are image barriers not submitted!" );
    iassertm( num_vk_buffer_barriers == 0, "There are buffer barriers not submitted!" );
    num_vk_image_barriers = 0;
    num_vk_buffer_barriers = 0;
}

void CommandBuffer::init( GpuDevice* gpu ) {

    gpu_device = gpu;
    num_vk_image_barriers = 0;
    num_vk_buffer_barriers = 0;

    // Allocate
    vk_image_barriers = ( VkImageMemoryBarrier2* )ialloc( sizeof( VkImageMemoryBarrier2 ) * k_max_image_outputs, gpu->allocator );
    vk_buffer_barriers = ( VkBufferMemoryBarrier2* )ialloc( sizeof( VkBufferMemoryBarrier2 ) * k_max_image_outputs, gpu->allocator );

    reset();
}

void CommandBuffer::shutdown() {

    is_recording = false;

    reset();

    ifree( vk_image_barriers, gpu_device->allocator );
    ifree( vk_buffer_barriers, gpu_device->allocator );
}

void CommandBuffer::begin_pass( Span<const TextureHandle> render_targets,
                                Span<const LoadOperation::Enum> load_operations,
                                Span<const ClearColor> clear_values,
                                TextureHandle depth,
                                LoadOperation::Enum depth_load_operation,
                                ClearDepthStencil depth_stencil_clear ) {

    Array<VkRenderingAttachmentInfoKHR> color_attachments_info;

    BookmarkAllocator* temporary_allocator = g_memory->get_thread_allocator();
    u64 marker = temporary_allocator->get_marker();
    color_attachments_info.init( temporary_allocator, (u32)render_targets.size, ( u32 )render_targets.size );
    memset( color_attachments_info.data, 0, sizeof( VkRenderingAttachmentInfoKHR ) * render_targets.size );

    frame_buffer_width = 0;
    frame_buffer_height = 0;

    inside_pass = true;

    for ( u32 a = 0; a < ( u32 )render_targets.size; ++a ) {
        Texture* texture = gpu_device->textures.get_cold( render_targets[a] );
        VulkanTexture* vk_texture = gpu_device->textures.get_hot( render_targets[ a ] );

        iassert( vk_texture->state == ResourceState::RenderTarget );

        if ( a == 0 ) {
            frame_buffer_width = texture->width;
            frame_buffer_height = texture->height;
        }
        else {
            iassert( frame_buffer_width == texture->width );
            iassert( frame_buffer_height == texture->height );
        }

        VkAttachmentLoadOp load_op;
        switch ( load_operations[ a ] ) {
            case LoadOperation::Load:
                load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
                break;
            case LoadOperation::Clear:
                load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                break;
            default:
                load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                break;
        }

        VkRenderingAttachmentInfoKHR& color_attachment_info = color_attachments_info[ a ];
        color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        color_attachment_info.imageView = vk_texture->vk_image_view;
        color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;// gpu_device->synchronization2_extension_present ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
        color_attachment_info.loadOp = load_op;
        color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        if ( load_op == VK_ATTACHMENT_LOAD_OP_CLEAR ) {
            color_attachment_info.clearValue.color.float32[ 0 ] = clear_values[ a ].rgba[ 0 ];
            color_attachment_info.clearValue.color.float32[ 1 ] = clear_values[ a ].rgba[ 1 ];
            color_attachment_info.clearValue.color.float32[ 2 ] = clear_values[ a ].rgba[ 2 ];
            color_attachment_info.clearValue.color.float32[ 3 ] = clear_values[ a ].rgba[ 3 ];
        }
        else {
            color_attachment_info.clearValue = {};
        }
    }

    VkRenderingAttachmentInfoKHR depth_attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };

    if ( depth.is_valid() ) {
        Texture* texture = gpu_device->textures.get_cold( depth );
        VulkanTexture* vk_texture = gpu_device->textures.get_hot( depth );
        iassert( vk_texture->state == ResourceState::RenderTarget );
        iassert( frame_buffer_width == texture->width );
        iassert( frame_buffer_height == texture->height );

        VkAttachmentLoadOp load_op;
        switch ( depth_load_operation ) {
            case LoadOperation::Load:
                load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
                break;
            case LoadOperation::Clear:
                load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                break;
            default:
                load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                break;
        }

        depth_attachment_info.imageView = vk_texture->vk_image_view;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR;
        depth_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
        depth_attachment_info.loadOp = load_op;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment_info.clearValue.depthStencil.depth = depth_stencil_clear.depth_value;
        depth_attachment_info.clearValue.depthStencil.stencil = depth_stencil_clear.stencil_value;
    }

    VkRenderingInfoKHR rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
    rendering_info.flags = 0;
    rendering_info.layerCount = 1;//framebuffer->layers;
    rendering_info.viewMask = 0;// render_pass->multiview_mask;
    rendering_info.colorAttachmentCount = ( u32 )render_targets.size;
    rendering_info.pColorAttachments = color_attachments_info.data;
    rendering_info.renderArea = { 0,0, frame_buffer_width, frame_buffer_height };
    rendering_info.pDepthAttachment = depth.is_valid() ? &depth_attachment_info : nullptr;
    rendering_info.pStencilAttachment = nullptr;

    vkCmdBeginRenderingKHR( vk_command_buffer, &rendering_info );

    temporary_allocator->free_marker( marker );
}

void CommandBuffer::end_render_pass() {

    vkCmdEndRenderingKHR( vk_command_buffer );

    inside_pass = false;
    frame_buffer_width = 0;
    frame_buffer_height = 0;
}

void CommandBuffer::bind_pipeline( PipelineHandle handle_ ) {

    VulkanPipeline* pipeline = gpu_device->pipelines.get_hot( handle_ );
    vkCmdBindPipeline( vk_command_buffer, ( VkPipelineBindPoint )pipeline->vk_bind_point, pipeline->vk_pipeline );

    // Cache pipeline
    current_pipeline = pipeline;
}

void CommandBuffer::bind_vertex_buffer( BufferHandle handle_, u32 binding, u32 offset ) {

    VulkanBuffer* buffer = gpu_device->buffers.get_hot( handle_ );
    VkDeviceSize offsets[] = { offset };

    VkBuffer vk_buffer = buffer->vk_buffer;

    vkCmdBindVertexBuffers( vk_command_buffer, binding, 1, &vk_buffer, offsets );
}

void CommandBuffer::bind_vertex_buffers( BufferHandle* handles, u32 first_binding, u32 binding_count, u32* offsets_ ) {

    iassert( false );
    //VkBuffer vk_buffers[ 8 ];
    //VkDeviceSize offsets[ 8 ];

    //for ( u32 i = 0; i < binding_count; ++i ) {
    //    Buffer* buffer = gpu_device->access_buffer( handles[i] );

    //    VkBuffer vk_buffer = buffer->vk_buffer;
    //    // TODO: add global vertex buffer ?
    //    if ( buffer->parent_buffer.index != k_invalid_index ) {
    //        Buffer* parent_buffer = gpu_device->access_buffer( buffer->parent_buffer );
    //        vk_buffer = parent_buffer->vk_buffer;
    //        offsets[ i ] = buffer->global_offset;
    //    }
    //    else {
    //        offsets[ i ] = offsets_[ i ];
    //    }

    //    vk_buffers[ i ] = vk_buffer;
    //}

    //vkCmdBindVertexBuffers( vk_command_buffer, first_binding, binding_count, vk_buffers, offsets );
}

void CommandBuffer::bind_index_buffer( BufferHandle handle_, u32 offset_, IndexType::Enum index_type ) {

    VulkanBuffer* buffer = gpu_device->buffers.get_hot( handle_ );

    VkBuffer vk_buffer = buffer->vk_buffer;
    VkDeviceSize offset = offset_;

    vkCmdBindIndexBuffer( vk_command_buffer, vk_buffer, offset, VK_INDEX_TYPE_UINT16/* to_vk_index_type( index_type )*/ );
}

void CommandBuffer::bind_descriptor_set( Span<const DescriptorSetHandle> handles, Span<const u32> offsets ) {

    VkDescriptorSet vk_descriptor_sets[ 4 ];
    for (u32 i = 0; i < handles.size; ++ i) {
        VulkanDescriptorSet* ds = gpu_device->descriptor_sets.get_hot( handles[ i ] );
        vk_descriptor_sets[ i ] = ds->vk_descriptor_set;
    }

    const u32 k_first_set = 0;
    vkCmdBindDescriptorSets( vk_command_buffer, ( VkPipelineBindPoint )current_pipeline->vk_bind_point, current_pipeline->vk_pipeline_layout, k_first_set,
                             ( u32 )handles.size, vk_descriptor_sets, ( u32 )offsets.size, offsets.data );
}

void CommandBuffer::set_framebuffer_viewport() {

    iassert( inside_pass );

    VkViewport vk_viewport;
    vk_viewport.x = 0.f;
    vk_viewport.width = frame_buffer_width * 1.f;
    // Invert Y with negative height and proper offset - Vulkan has unique Clipping Y.
    vk_viewport.y = frame_buffer_height * 1.f;
    vk_viewport.height = frame_buffer_height * -1.f;
    vk_viewport.minDepth = 0.0f;
    vk_viewport.maxDepth = 1.0f;

    vkCmdSetViewport( vk_command_buffer, 0, 1, &vk_viewport );
}

void CommandBuffer::set_viewport( const Viewport& viewport ) {

    iassert( inside_pass );

    VkViewport vk_viewport;
    vk_viewport.x = viewport.rect.x * 1.f;
    vk_viewport.width = viewport.rect.width * 1.f;
    // Invert Y with negative height and proper offset - Vulkan has unique Clipping Y.
    vk_viewport.y = viewport.rect.height * 1.f - viewport.rect.y;
    vk_viewport.height = -viewport.rect.height * 1.f;
    vk_viewport.minDepth = viewport.min_depth;
    vk_viewport.maxDepth = viewport.max_depth;

    vkCmdSetViewport( vk_command_buffer, 0, 1, &vk_viewport);
}

void CommandBuffer::set_framebuffer_scissor() {
    iassert( inside_pass );

    VkRect2D vk_scissor;
    vk_scissor.offset.x = 0;
    vk_scissor.offset.y = 0;
    vk_scissor.extent.width = frame_buffer_width;
    vk_scissor.extent.height = frame_buffer_height;

    vkCmdSetScissor( vk_command_buffer, 0, 1, &vk_scissor );
}

void CommandBuffer::set_scissor( const Rect2DInt& rect ) {

    iassert( inside_pass );

    VkRect2D vk_scissor;
    vk_scissor.offset.x = rect.x;
    vk_scissor.offset.y = rect.y;
    vk_scissor.extent.width = rect.width;
    vk_scissor.extent.height = rect.height;

    vkCmdSetScissor( vk_command_buffer, 0, 1, &vk_scissor );
}

void CommandBuffer::push_constants( PipelineHandle pipeline, u32 offset, u32 size, void* data ) {
    VulkanPipeline* pipeline_ = gpu_device->pipelines.get_hot( pipeline );
    vkCmdPushConstants( vk_command_buffer, pipeline_->vk_pipeline_layout, VK_SHADER_STAGE_ALL, offset, size, data );
}

void CommandBuffer::draw( TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count ) {
    vkCmdDraw( vk_command_buffer, vertex_count, instance_count, first_vertex, first_instance );
}

void CommandBuffer::draw_indexed( TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance ) {
    vkCmdDrawIndexed( vk_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance );
}

void CommandBuffer::draw_indirect( BufferHandle buffer_handle, u32 draw_count, u32 offset, u32 stride ) {

    VulkanBuffer* buffer = gpu_device->buffers.get_hot( buffer_handle );

    VkBuffer vk_buffer = buffer->vk_buffer;
    VkDeviceSize vk_offset = offset;

    vkCmdDrawIndirect( vk_command_buffer, vk_buffer, vk_offset, draw_count, stride );
}

void CommandBuffer::draw_indirect_count( BufferHandle argument_buffer, u32 argument_offset, BufferHandle count_buffer, u32 count_offset, u32 max_draws, u32 stride ) {
    VulkanBuffer* argument_buffer_ = gpu_device->buffers.get_hot( argument_buffer );
    VulkanBuffer* count_buffer_ = gpu_device->buffers.get_hot( count_buffer );

    vkCmdDrawIndirectCount( vk_command_buffer, argument_buffer_->vk_buffer, argument_offset, count_buffer_->vk_buffer, count_offset, max_draws, stride );
}

void CommandBuffer::draw_indexed_indirect( BufferHandle buffer_handle, u32 draw_count, u32 offset, u32 stride ) {
    VulkanBuffer* buffer = gpu_device->buffers.get_hot( buffer_handle );

    VkBuffer vk_buffer = buffer->vk_buffer;
    VkDeviceSize vk_offset = offset;

    vkCmdDrawIndexedIndirect( vk_command_buffer, vk_buffer, vk_offset, draw_count, stride );
}

void CommandBuffer::draw_mesh_task( u32 task_count ) {

    vkCmdDrawMeshTasksEXT( vk_command_buffer, task_count, 1, 1 );
}

void CommandBuffer::draw_mesh_task_indirect( BufferHandle argument_buffer, u32 argument_offset, u32 command_count, u32 stride ) {
    VulkanBuffer* argument_buffer_ = gpu_device->buffers.get_hot( argument_buffer );

    vkCmdDrawMeshTasksIndirectEXT( vk_command_buffer, argument_buffer_->vk_buffer, argument_offset, command_count, stride );
}

void CommandBuffer::draw_mesh_task_indirect_count( BufferHandle argument_buffer, u32 argument_offset, BufferHandle count_buffer, u32 count_offset, u32 max_draws, u32 stride ) {
    VulkanBuffer* argument_buffer_ = gpu_device->buffers.get_hot( argument_buffer );
    VulkanBuffer* count_buffer_ = gpu_device->buffers.get_hot( count_buffer );

    vkCmdDrawMeshTasksIndirectCountEXT( vk_command_buffer, argument_buffer_->vk_buffer, argument_offset, count_buffer_->vk_buffer, count_offset, max_draws, stride );
}

void CommandBuffer::dispatch_1d( u32 total_threads_x, u32 workgroup_size_x ) {
    vkCmdDispatch( vk_command_buffer, ceilu32( total_threads_x * 1.0 / workgroup_size_x ), 1, 1 );
}

void CommandBuffer::dispatch_2d( u32 total_threads_x, u32 total_threads_y, u32 workgroup_size_x, u32 workgroup_size_y ) {
    vkCmdDispatch( vk_command_buffer, ceilu32( total_threads_x * 1.0 / workgroup_size_x ), ceilu32( total_threads_y * 1.0 / workgroup_size_y ), 1 );
}

void CommandBuffer::dispatch_3d( u32 total_threads_x, u32 total_threads_y, u32 total_threads_z, u32 workgroup_size_x, u32 workgroup_size_y, u32 workgroup_size_z ) {
    vkCmdDispatch( vk_command_buffer, ceilu32( total_threads_x * 1.0 / workgroup_size_x ), ceilu32( total_threads_y * 1.0 / workgroup_size_y ), ceilu32( total_threads_z * 1.0 / workgroup_size_z ) );
}

void CommandBuffer::dispatch_indirect( BufferHandle buffer_handle, u32 offset ) {
    VulkanBuffer* buffer = gpu_device->buffers.get_hot( buffer_handle );

    VkBuffer vk_buffer = buffer->vk_buffer;
    VkDeviceSize vk_offset = offset;

    vkCmdDispatchIndirect( vk_command_buffer, vk_buffer, vk_offset );
}

void CommandBuffer::trace_rays( PipelineHandle pipeline_, u32 width, u32 height, u32 depth ) {
    iassert( false );
    //Pipeline* pipeline = gpu_device->access_pipeline( pipeline_ );

    //u32 shader_group_handle_size = gpu_device->ray_tracing_pipeline_properties.shaderGroupHandleSize;

    //// NOTE(marco): always 0 in the shader table for now
    //VkStridedDeviceAddressRegionKHR raygen_table{ };
    //raygen_table.deviceAddress = gpu_device->get_buffer_device_address( pipeline->shader_binding_table_raygen );
    //raygen_table.stride = shader_group_handle_size;
    //raygen_table.size = shader_group_handle_size;

    //// NOTE(marco): always 1 in the shader table for now
    //VkStridedDeviceAddressRegionKHR hit_table{ };
    //hit_table.deviceAddress = gpu_device->get_buffer_device_address( pipeline->shader_binding_table_hit );
    //hit_table.stride = shader_group_handle_size;
    //hit_table.size = shader_group_handle_size;

    //// NOTE(marco): always 2 in the shader table for now
    //VkStridedDeviceAddressRegionKHR miss_table{ };
    //miss_table.deviceAddress = gpu_device->get_buffer_device_address( pipeline->shader_binding_table_miss );
    //miss_table.stride = shader_group_handle_size;
    //miss_table.size = shader_group_handle_size;

    //// NOTE(marco): unused for now
    //VkStridedDeviceAddressRegionKHR callable_table{ };

    //gpu_device->vkCmdTraceRaysKHR( vk_command_buffer, &raygen_table, &miss_table, &hit_table, &callable_table, width, height, depth );
}

void CommandBuffer::global_debug_barrier() {

    VkMemoryBarrier2KHR barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR };

    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;

    VkDependencyInfoKHR dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2KHR( vk_command_buffer, &dependency_info );
}

void CommandBuffer::submit_barriers( Span<const TextureBarrier> texture_barriers, Span<const BufferBarrier> buffer_barriers ) {

    for ( u32 b = 0; b < ( u32 )texture_barriers.size; ++b ) {
        const TextureBarrier& texture_barrier = texture_barriers[ b ];

        VulkanTexture* vk_texture = gpu_device->textures.get_hot( texture_barrier.texture );
        if ( vk_texture->state == texture_barrier.new_state ) {
            continue;
        }

        iassert( num_vk_image_barriers < k_max_image_outputs );

        VkImageMemoryBarrier2* barrier = &vk_image_barriers[ num_vk_image_barriers++ ];
        gpu_device->fill_image_barrier( barrier, texture_barrier.texture, texture_barrier.new_state,
                                        texture_barrier.mip_level,
                                        texture_barrier.mip_count, 0, 1,
                                        gpu_device->queue_indices[ texture_barrier.source_queue ],
                                        gpu_device->queue_indices[ texture_barrier.destination_queue ],
                                        texture_barrier.source_queue,
                                        texture_barrier.destination_queue );
    }

    for ( u32 b = 0; b < (u32)buffer_barriers.size; ++ b) {
        const BufferBarrier& buffer_barrier = buffer_barriers[ b ];
        Buffer* buffer = gpu_device->buffers.get_cold( buffer_barrier.buffer );
        if ( buffer->state == buffer_barrier.new_state ) {
            continue;
        }

        VkBufferMemoryBarrier2* barrier = &vk_buffer_barriers[ num_vk_buffer_barriers++ ];
        gpu_device->fill_buffer_barrier( barrier, buffer_barrier.buffer,
                                         buffer_barrier.new_state,
                                         buffer_barrier.offset, buffer_barrier.size );
    }

    if ( num_vk_image_barriers == 0 && num_vk_buffer_barriers == 0 ) {
        return;
    }

    // Submit all barriers
    VkDependencyInfoKHR dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
    dependency_info.imageMemoryBarrierCount = num_vk_image_barriers;
    dependency_info.pImageMemoryBarriers = vk_image_barriers;
    dependency_info.bufferMemoryBarrierCount = num_vk_buffer_barriers;
    dependency_info.pBufferMemoryBarriers = vk_buffer_barriers;

    vkCmdPipelineBarrier2KHR( vk_command_buffer, &dependency_info );

    // Restore barrier count to 0
    num_vk_image_barriers = 0;
    num_vk_buffer_barriers = 0;
}

void CommandBuffer::clear_color_image( TextureHandle texture, ClearColor clear_color ) {
    VulkanTexture* vk_texture = gpu_device->textures.get_hot( texture );

    VkImageSubresourceRange range{ };
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseArrayLayer= 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;

    VkClearColorValue vk_clear_color;
    vk_clear_color.float32[ 0 ] = clear_color.rgba[ 0 ];
    vk_clear_color.float32[ 1 ] = clear_color.rgba[ 1 ];
    vk_clear_color.float32[ 2 ] = clear_color.rgba[ 2 ];
    vk_clear_color.float32[ 3 ] = clear_color.rgba[ 3 ];

    vkCmdClearColorImage( vk_command_buffer, vk_texture->vk_image, VK_IMAGE_LAYOUT_GENERAL, &vk_clear_color, 1, &range);
}

void CommandBuffer::fill_buffer( BufferHandle buffer, u32 offset, u32 size, u32 data ) {
    VulkanBuffer* vk_buffer = gpu_device->buffers.get_hot( buffer );
    Buffer* buffer_ = gpu_device->buffers.get_cold( buffer );

    vkCmdFillBuffer( vk_command_buffer, vk_buffer->vk_buffer, VkDeviceSize( offset ), size ? VkDeviceSize( size ) : VkDeviceSize( buffer_->size ), data);
}

void CommandBuffer::push_marker( StringView name ) {

    GPUTimeQuery* time_query = time_query_tree.push( name );
    vkCmdWriteTimestamp( vk_command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk_time_query_pool, time_query->start_query_index );

    if ( !gpu_device->debug_utils_extension_present )
        return;

    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name.data;
    label.color[ 0 ] = 1.0f;
    label.color[ 1 ] = 1.0f;
    label.color[ 2 ] = 1.0f;
    label.color[ 3 ] = 1.0f;
    vkCmdBeginDebugUtilsLabelEXT( vk_command_buffer, &label );
}

void CommandBuffer::pop_marker() {

    GPUTimeQuery* time_query = time_query_tree.pop();
    vkCmdWriteTimestamp( vk_command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk_time_query_pool, time_query->end_query_index );

    if ( !gpu_device->debug_utils_extension_present )
        return;

    vkCmdEndDebugUtilsLabelEXT( vk_command_buffer );
}

void CommandBuffer::upload_texture_data( TextureHandle texture_handle, void* texture_data, BufferHandle staging_buffer_handle, sizet staging_buffer_offset ) {

    Texture* texture = gpu_device->textures.get_cold( texture_handle );
    VulkanTexture* vk_texture = gpu_device->textures.get_hot( texture_handle );

    Buffer* buffer = gpu_device->buffers.get_cold( staging_buffer_handle );
    VulkanBuffer* vk_buffer = gpu_device->buffers.get_hot( staging_buffer_handle );

    u32 image_size = (u32)GpuUtils::calculate_texture_size( texture );

    // Copy buffer_data to staging buffer
    memcpy( buffer->mapped_data + staging_buffer_offset, texture_data, static_cast< size_t >( image_size ) );

    VkBufferImageCopy region = {};
    region.bufferOffset = staging_buffer_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { texture->width, texture->height, texture->depth };

    // Pre copy memory barrier to perform layout transition
    submit_barriers( {{texture_handle, ResourceState::CopyDest, 0, 1, QueueType::Transfer, QueueType::Transfer}}, {});

    // Copy from the staging buffer to the image
    vkCmdCopyBufferToImage( vk_command_buffer, vk_buffer->vk_buffer, vk_texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    // Post copy memory barrier
    submit_barriers( { {texture_handle, ResourceState::CopySource, 0, 1, QueueType::Transfer, QueueType::Graphics} }, {} );
}

void CommandBuffer::copy_texture( TextureHandle src_, TextureHandle dst_, ResourceState::Enum dst_state ) {
    Texture* src = gpu_device->textures.get_cold( src_ );
    VulkanTexture* vk_src = gpu_device->textures.get_hot( src_ );

    Texture* dst = gpu_device->textures.get_cold( dst_ );
    VulkanTexture* vk_dst = gpu_device->textures.get_hot( dst_ );

    bool src_is_depth = src->vk_format == VK_FORMAT_D32_SFLOAT;
    bool dst_is_depth = dst->vk_format == VK_FORMAT_D32_SFLOAT;

    // NOTE(marco): can't copy between depth and color
    iassert( src_is_depth == dst_is_depth );

    VkImageCopy region = {};
    region.srcSubresource.aspectMask = src_is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;

    region.dstSubresource.aspectMask = dst_is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    region.dstOffset = { 0, 0, 0 };
    region.extent = { src->width, src->height, src->depth };

    // Copy from the staging buffer to the image
    submit_barriers( { {src_, ResourceState::CopySource, 0, 1},
                     {dst_, ResourceState::CopyDest, 0, 1 } }, {} );

    vkCmdCopyImage( vk_command_buffer, vk_src->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk_dst->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    submit_barriers( {{dst_, dst_state, 0, 1}}, {} );

    // TODO: add back mipmap handling
    //// Prepare first mip to create lower mipmaps
    //if ( dst->mip_level_count > 1 ) {
    //    RASSERT( !dst_is_depth );
    //    util_add_image_barrier( gpu_device, vk_command_buffer, dst, RESOURCE_STATE_COPY_SOURCE, 0, 1, dst_is_depth );
    //}

    //i32 w = dst->width;
    //i32 h = dst->height;

    //for ( int mip_index = 1; mip_index < dst->mip_level_count; ++mip_index ) {
    //    util_add_image_barrier( gpu_device, vk_command_buffer, dst->vk_image, old_state, RESOURCE_STATE_COPY_DEST, mip_index, 1, dst_is_depth );

    //    VkImageBlit blit_region{ };
    //    blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //    blit_region.srcSubresource.mipLevel = mip_index - 1;
    //    blit_region.srcSubresource.baseArrayLayer = 0;
    //    blit_region.srcSubresource.layerCount = 1;

    //    blit_region.srcOffsets[ 0 ] = { 0, 0, 0 };
    //    blit_region.srcOffsets[ 1 ] = { w, h, 1 };

    //    w /= 2;
    //    h /= 2;

    //    blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //    blit_region.dstSubresource.mipLevel = mip_index;
    //    blit_region.dstSubresource.baseArrayLayer = 0;
    //    blit_region.dstSubresource.layerCount = 1;

    //    blit_region.dstOffsets[ 0 ] = { 0, 0, 0 };
    //    blit_region.dstOffsets[ 1 ] = { w, h, 1 };

    //    vkCmdBlitImage( vk_command_buffer, dst->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_LINEAR );

    //    // Prepare current mip for next level
    //    util_add_image_barrier( gpu_device, vk_command_buffer, dst->vk_image, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_SOURCE, mip_index, 1, dst_is_depth );
    //}

    //// Transition
    //util_add_image_barrier( gpu_device, vk_command_buffer, dst, dst_state, 0, dst->mip_level_count, dst_is_depth );
}

void CommandBuffer::copy_texture( TextureHandle src_, TextureSubResource src_sub, TextureHandle dst_, TextureSubResource dst_sub, ResourceState::Enum dst_state ) {
    //Texture* src = gpu_device->access_texture( src_ );
    //Texture* dst = gpu_device->access_texture( dst_ );

    //const bool src_is_depth = TextureFormat::is_depth_only( src->vk_format );
    //const bool dst_is_depth = TextureFormat::is_depth_only( dst->vk_format );

    //VkImageCopy region = {};
    //region.srcSubresource.aspectMask = src_is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    //region.srcSubresource.mipLevel = src_sub.mip_base_level;
    //region.srcSubresource.baseArrayLayer = src_sub.array_base_layer;
    //region.srcSubresource.layerCount = src_sub.array_layer_count;

    //region.dstSubresource.aspectMask = dst_is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;;
    //region.dstSubresource.mipLevel = dst_sub.mip_base_level;
    //region.dstSubresource.baseArrayLayer = dst_sub.array_base_layer;
    //region.dstSubresource.layerCount = dst_sub.array_layer_count;

    //region.dstOffset = { 0, 0, 0 };
    //region.extent = { src->width, src->height, src->depth };

    //// Copy from the staging buffer to the image
    //util_add_image_barrier( gpu_device, vk_command_buffer, src, RESOURCE_STATE_COPY_SOURCE, 0, 1, src_is_depth );
    //// TODO(marco): maybe we need a state per mip?
    //ResourceState old_state = dst->state;
    //util_add_image_barrier( gpu_device, vk_command_buffer, dst, RESOURCE_STATE_COPY_DEST, 0, 1, dst_is_depth );

    //vkCmdCopyImage( vk_command_buffer, src->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    //// Prepare first mip to create lower mipmaps
    //if ( dst->mip_level_count > 1 ) {
    //    util_add_image_barrier( gpu_device, vk_command_buffer, dst, RESOURCE_STATE_COPY_SOURCE, 0, 1, src_is_depth );
    //}

    //i32 w = dst->width;
    //i32 h = dst->height;

    //for ( int mip_index = 1; mip_index < dst->mip_level_count; ++mip_index ) {
    //    util_add_image_barrier( gpu_device, vk_command_buffer, dst->vk_image, old_state, RESOURCE_STATE_COPY_DEST, mip_index, 1, dst_is_depth );

    //    VkImageBlit blit_region{ };
    //    blit_region.srcSubresource.aspectMask = src_is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    //    blit_region.srcSubresource.mipLevel = mip_index - 1;
    //    blit_region.srcSubresource.baseArrayLayer = 0;
    //    blit_region.srcSubresource.layerCount = 1;

    //    blit_region.srcOffsets[ 0 ] = { 0, 0, 0 };
    //    blit_region.srcOffsets[ 1 ] = { w, h, 1 };

    //    w /= 2;
    //    h /= 2;

    //    blit_region.dstSubresource.aspectMask = dst_is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;;
    //    blit_region.dstSubresource.mipLevel = mip_index;
    //    blit_region.dstSubresource.baseArrayLayer = 0;
    //    blit_region.dstSubresource.layerCount = 1;

    //    blit_region.dstOffsets[ 0 ] = { 0, 0, 0 };
    //    blit_region.dstOffsets[ 1 ] = { w, h, 1 };

    //    vkCmdBlitImage( vk_command_buffer, dst->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_LINEAR );

    //    // Prepare current mip for next level
    //    util_add_image_barrier( gpu_device, vk_command_buffer, dst->vk_image, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_SOURCE, mip_index, 1, false );
    //}

    //// Transition
    //util_add_image_barrier( gpu_device, vk_command_buffer, dst, dst_state, 0, dst->mip_level_count, dst_is_depth );
}

void CommandBuffer::copy_buffer( BufferHandle src, sizet src_offset, BufferHandle dst, sizet dst_offset, sizet size ) {
    /*Buffer* src_buffer = gpu_device->access_buffer( src );
    Buffer* dst_buffer = gpu_device->access_buffer( dst );

    VkBufferCopy copy_region{ };
    copy_region.srcOffset = src_offset;
    copy_region.dstOffset = dst_offset;
    copy_region.size = size;

    vkCmdCopyBuffer( vk_command_buffer, src_buffer->vk_buffer, dst_buffer->vk_buffer, 1, &copy_region );*/
}

void CommandBuffer::upload_buffer_data( BufferHandle buffer_handle, void* buffer_data, BufferHandle staging_buffer_handle, sizet staging_buffer_offset ) {

    //Buffer* buffer = gpu_device->access_buffer( buffer_handle );
    //Buffer* staging_buffer = gpu_device->access_buffer( staging_buffer_handle );
    //u32 copy_size = buffer->size;

    //// Copy buffer_data to staging buffer
    //memcpy( staging_buffer->mapped_data + staging_buffer_offset, buffer_data, static_cast< size_t >( copy_size ) );

    //VkBufferCopy region{};
    //region.srcOffset = staging_buffer_offset;
    //region.dstOffset = 0;
    //region.size = copy_size;

    //vkCmdCopyBuffer( vk_command_buffer, staging_buffer->vk_buffer, buffer->vk_buffer, 1, &region );

    //util_add_buffer_barrier_ext( gpu_device, vk_command_buffer, buffer->vk_buffer, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_UNDEFINED,
    //                             copy_size, gpu_device->vulkan_transfer_queue_family, gpu_device->vulkan_main_queue_family,
    //                             QueueType::CopyTransfer, QueueType::Graphics );
}

void CommandBuffer::upload_buffer_data( BufferHandle src_, BufferHandle dst_ ) {
    /*Buffer* src = gpu_device->access_buffer( src_ );
    Buffer* dst = gpu_device->access_buffer( dst_ );

    RASSERT( src->size == dst->size );

    u32 copy_size = src->size;

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = copy_size;

    vkCmdCopyBuffer( vk_command_buffer, src->vk_buffer, dst->vk_buffer, 1, &region );*/
}

void CommandBufferManager::init( GpuDevice* gpu, u32 max_command_buffers ) {

    gpu_device = gpu;

    // TODO: configurable number
    queries_per_pool = 50 * 2u;

    max_command_buffers_per_queue[ QueueType::Graphics ] = 3;
    max_command_buffers_per_queue[ QueueType::Compute ] = 1;
    max_command_buffers_per_queue[ QueueType::Transfer ] = 1;

    const u32 max_buffers_per_frame = get_max_buffers_per_frame();
    // Allocate time queries
    const u32 total_queries = queries_per_pool * max_buffers_per_frame * k_max_frames;
    time_queries.init( gpu->allocator, total_queries, total_queries );

    // Create command buffer pools.
    VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    // Create command buffers.
    VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    vk_command_pools.init( gpu->allocator, max_buffers_per_frame * k_max_frames,
                           max_buffers_per_frame * k_max_frames );

    command_buffers.init( gpu->allocator, max_buffers_per_frame * k_max_frames,
                                          max_buffers_per_frame * k_max_frames );


    // TODO: calculate offsets
    // Allocate pools linked to queue families
    u32 buffers_offset[ 3 ] = { 3, 4, 5 };
    // TODO: calculate queue types per absolute index
    QueueType::Enum queue_types[] = { QueueType::Graphics, QueueType::Graphics,
            QueueType::Graphics, QueueType::Compute, QueueType::Transfer };


    // Create command buffers
    for ( u32 q = 0; q < max_buffers_per_frame; ++q ) {

        cmd_pool_info.queueFamilyIndex = gpu->queue_indices[ queue_types[ q ] ];

        for ( u32 i = 0; i < k_max_frames; ++i ) {

            const u32 index = q + i * max_buffers_per_frame;

            vkCreateCommandPool( gpu->vk_device, &cmd_pool_info, gpu->vk_allocation_callbacks, &vk_command_pools[ index ]);

            CommandBuffer* command_buffer = &command_buffers[ index ];
            command_buffer->init( gpu );
            command_buffer->type = queue_types[ q ];

            allocateInfo.commandPool = vk_command_pools[ index ];
            vkAllocateCommandBuffers( gpu->vk_device, &allocateInfo, &command_buffer->vk_command_buffer );

            // Create timestamp query pool used for GPU timings.
            VkQueryPoolCreateInfo timestamp_pool_info{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_TIMESTAMP, queries_per_pool, 0 };
            vkCreateQueryPool( gpu->vk_device, &timestamp_pool_info, gpu->vk_allocation_callbacks, &command_buffer->vk_time_query_pool );

            // Setup time query view
            command_buffer->time_query_tree.set_queries( &time_queries[ index * queries_per_pool ], queries_per_pool );
        }
    }

    current_frame = 0;
}

void CommandBufferManager::shutdown() {

    const u32 max_buffers = get_max_buffers_per_frame() * k_max_frames;

    for ( u32 q = 0; q < max_buffers; ++q ) {

        CommandBuffer* command_buffer = &command_buffers[ q ];
        command_buffer->shutdown();

        vkDestroyCommandPool( gpu_device->vk_device, vk_command_pools[ q ], gpu_device->vk_allocation_callbacks );

        vkDestroyQueryPool( gpu_device->vk_device, command_buffer->vk_time_query_pool, gpu_device->vk_allocation_callbacks );
    }

    time_queries.shutdown();
    command_buffers.shutdown();
    vk_command_pools.shutdown();
}

void CommandBufferManager::free_unused_buffers( u32 current_frame_ ) {

    current_frame = current_frame_;

    for ( u32 q = 0; q < QueueType::Count; ++q ) {
        used_command_buffers_per_queue[ q ] = 0;
    }

    const u32 max_buffers_per_frame = get_max_buffers_per_frame();

    for ( u32 q = 0; q < max_buffers_per_frame; ++q ) {

        const u32 index = q + current_frame_ * max_buffers_per_frame;

        vkResetCommandPool( gpu_device->vk_device, vk_command_pools[ index ], 0 );

        CommandBuffer* command_buffer = &command_buffers[ index ];
        command_buffer->reset();

        vkResetQueryPool( gpu_device->vk_device, command_buffer->vk_time_query_pool, 0, queries_per_pool );

        command_buffer->time_query_tree.reset();
    }
}

static void command_buffer_begin( CommandBuffer* command_buffer ) {

    if ( !command_buffer->is_recording ) {

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        //
        vkBeginCommandBuffer( command_buffer->vk_command_buffer, &beginInfo );

        command_buffer->is_recording = true;
    }

}

CommandBuffer* CommandBufferManager::get_graphics_command_buffer() {
    const u32 max_buffers_per_frame = get_max_buffers_per_frame();
    u32 q = used_command_buffers_per_queue[ QueueType::Graphics ]++;

    iassert( used_command_buffers_per_queue[ QueueType::Graphics ] <=
             max_command_buffers_per_queue[ QueueType::Graphics ] );

    const u32 index = q + current_frame * max_buffers_per_frame;
    CommandBuffer* cb = &command_buffers[ index ];
    command_buffer_begin( cb );

    return cb;
}

CommandBuffer* CommandBufferManager::get_active_graphics_command_buffer( u32 cb_index ) {
    const u32 max_buffers_per_frame = get_max_buffers_per_frame();

    iassert( cb_index < used_command_buffers_per_queue[ QueueType::Graphics ] );

    u32 q = cb_index;
    const u32 index = q + current_frame * max_buffers_per_frame;

    CommandBuffer* cb = &command_buffers[ index ];
    // If not recording, begin command buffer
    command_buffer_begin( cb );

    return cb;
}

CommandBuffer* CommandBufferManager::get_compute_command_buffer() {
    const u32 max_buffers_per_frame = get_max_buffers_per_frame();
    u32 q = (used_command_buffers_per_queue[ QueueType::Compute ]++) +
            max_command_buffers_per_queue[QueueType::Graphics];

    iassert( used_command_buffers_per_queue[ QueueType::Compute ] <=
             max_command_buffers_per_queue[ QueueType::Compute ] );

    const u32 index = q + current_frame * max_buffers_per_frame;
    CommandBuffer* cb = &command_buffers[ index ];
    // If not recording, begin command buffer
    command_buffer_begin( cb );

    return cb;
}

CommandBuffer* CommandBufferManager::get_transfer_command_buffer() {
    const u32 max_buffers_per_frame = get_max_buffers_per_frame();
    u32 q = ( used_command_buffers_per_queue[ QueueType::Transfer ]++ ) +
        max_command_buffers_per_queue[ QueueType::Graphics ] +
        max_command_buffers_per_queue[ QueueType::Compute ];

    iassert( used_command_buffers_per_queue[ QueueType::Transfer ] <=
             max_command_buffers_per_queue[ QueueType::Transfer ] );

    const u32 index = q + current_frame * max_buffers_per_frame;
    CommandBuffer* cb = &command_buffers[ index ];
    // If not recording, begin command buffer
    command_buffer_begin( cb );

    return cb;
}

u32 CommandBufferManager::get_max_buffers_per_frame() const {

    return max_command_buffers_per_queue[ QueueType::Graphics ] +
        max_command_buffers_per_queue[ QueueType::Compute ] +
        max_command_buffers_per_queue[ QueueType::Transfer ];
}

Span<CommandBuffer> CommandBufferManager::get_command_buffer_span( u32 frame ) {
    iassert( frame < k_max_frames );
    const u32 max_buffers_per_frame = get_max_buffers_per_frame();
    const u32 starting_index = max_buffers_per_frame * frame;

    CommandBuffer* data = &command_buffers[ starting_index ];
    return Span<CommandBuffer>(data, (sizet)max_buffers_per_frame);
}

} // namespace idra
