/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/platform.hpp"
#include "kernel/string_view.hpp"
#include "kernel/pool.hpp"
#include "kernel/allocator.hpp"

#include "gpu/gpu_enums.hpp"
#include "gpu/gpu_resources.hpp"

#if defined (IDRA_VULKAN)

// Volk
#include "external/volk.h"

IDRA_VK_DEFINE_HANDLE( VmaAllocator )
IDRA_VK_DEFINE_HANDLE( VmaAllocation )

#endif // IDRA_VULKAN

namespace idra {

    // Forward declarations
    struct CommandBuffer;
    struct CommandBufferManager;

    // DrawStream /////////////////////////////////////////////////////////

    //
    //
    struct DrawStream {

    };

    // Dynamic buffer implementation
    // https://threadreaderapp.com/thread/1575469255168036864.html
    //
    // Texture data upload example
    // https://threadreaderapp.com/thread/1575022317066821632.html
    //
    //  Bind groups
    // https://threadreaderapp.com/thread/1536244905189814272.html
    // https://threadreaderapp.com/thread/1535264435551477764.html
    // https://threadreaderapp.com/thread/1536780270216663043.html
    //
    // Resource manager
    // https://threadreaderapp.com/thread/1535175559067713536.html
    //
    // Resources
    // https://threadreaderapp.com/thread/1534867791815315463.html

    //

    //
    //
    struct GpuDescriptorPoolCreation {

        u16                             samplers = 16;
        u16                             combined_image_samplers = 256;
        u16                             sampled_image = 16;
        u16                             storage_image = 64;
        u16                             uniform_texel_buffers = 1;
        u16                             storage_texel_buffers = 1;
        u16                             uniform_buffer = 64;
        u16                             storage_buffer = 64;
        u16                             uniform_buffer_dynamic = 8;
        u16                             storage_buffer_dynamic = 8;
        u16                             input_attachments = 64;

    }; // struct GpuDescriptorPoolCreation

    namespace DescriptorSetBindingsPools {

        enum Enum {
            _2,
            _4,
            _8,
            _16,
            _32,
            _Count
        };

        static u32 s_counts[] = { 2, 4, 8, 16, 32 };

    } // namespace DescriptorSetBindingsPools
    //
    //
    struct GpuResourcePoolCreation {

        u16                             buffers = 64;
        u16                             textures = 256;
        u16                             pipelines = 64;
        u16                             samplers = 16;
        u16                             descriptor_set_layouts = 64;
        u16                             descriptor_sets = 32;
        //u16                             render_passes = 256;
        //u16                             framebuffers = 256;
        u16                             command_buffers = 16;
        u16                             shaders = 96;

        // Granular sub-resources allocations
        u16                             graphics_shader_info = 32;  // arrays of two shader infos (one of VS, one for FS)
        u16                             compute_shader_info = 32;   // single shader info allocations
        u16                             ray_tracing_shader_info = 32; // all-stages arrays of shader infos

        u16                             descriptor_set_bindings_2 = 8;  // group of 2 bindings
        u16                             descriptor_set_bindings_4 = 8;  // group of 4 bindings
        u16                             descriptor_set_bindings_8 = 8;  // group of 8 bindings
        u16                             descriptor_set_bindings_16 = 8;  // group of 16 bindings
        u16                             descriptor_set_bindings_32 = 8;  // group of 32 bindings
    }; // struct GpuResourcePoolCreation


    //
    struct GpuDeviceCreation {

        GpuDescriptorPoolCreation       descriptor_pool_creation;
        GpuResourcePoolCreation         resource_pool_creation;

        Allocator*                      system_allocator    = nullptr;
        void*                           os_window_handle    = nullptr;

        StringView                      shader_folder_path;
    }; // struct GpuDeviceCreation

    // GpuDevice //////////////////////////////////////////////////////////
    struct GpuDevice {

        // Possible system init/shutdown style
        static GpuDevice*       init_system( const GpuDeviceCreation& creation );
        static void             shutdown_system( GpuDevice* instance );

        // Interface
        bool                    internal_init( const GpuDeviceCreation& creation );
        void                    internal_shutdown();

        void                    new_frame();
        
        void                    enqueue_command_buffer( CommandBuffer* command_buffer );
        void                    submit_transfer_work( CommandBuffer* command_buffer );

        void                    present();
        SwapchainStatus::Enum   update_swapchain();

        CommandBuffer*          acquire_new_command_buffer();
        CommandBuffer*          acquire_command_buffer( u32 index );
        CommandBuffer*          acquire_compute_command_buffer();
        CommandBuffer*          acquire_transfer_command_buffer();

        BufferHandle            create_buffer( const BufferCreation& creation );
        TextureHandle           create_texture( const TextureCreation& creation );
        TextureHandle           create_texture_view( const TextureViewCreation& creation );
        PipelineHandle          create_graphics_pipeline( const GraphicsPipelineCreation& creation );
        PipelineHandle          create_compute_pipeline( const ComputePipelineCreation& creation );
        SamplerHandle           create_sampler( const SamplerCreation& creation );
        DescriptorSetLayoutHandle create_descriptor_set_layout( const DescriptorSetLayoutCreation& creation );
        DescriptorSetHandle     create_descriptor_set( const DescriptorSetCreation& creation );
        ShaderStateHandle       create_graphics_shader_state( const GraphicsShaderStateCreation& creation );
        ShaderStateHandle       create_compute_shader_state( const ComputeShaderStateCreation& creation );

        void                    destroy_buffer( BufferHandle buffer );
        void                    destroy_texture( TextureHandle texture );
        void                    destroy_pipeline( PipelineHandle pipeline );
        void                    destroy_sampler( SamplerHandle sampler );
        void                    destroy_descriptor_set_layout( DescriptorSetLayoutHandle layout );
        void                    destroy_descriptor_set( DescriptorSetHandle set );
        void                    destroy_shader_state( ShaderStateHandle shader );

        void*                   map_buffer( BufferHandle buffer, u32 offset, u32 size );
        void                    unmap_buffer( BufferHandle buffer );

        void*                   dynamic_buffer_allocate( u32 size, u32 alignment, u32* dynamic_offset );

        template<typename T>
        T*                      dynamic_buffer_allocate( u32* dynamic_offset );

        BufferHandle            get_dynamic_buffer();

        void                    upload_texture_data( TextureHandle texture, void* data );

        void                    resize_texture( TextureHandle texture, u32 width, u32 height );
        void                    resize_texture_3d( TextureHandle texture, u32 width, u32 height, u32 depth );

#if defined (IDRA_VULKAN)
        
        // Local methods
        void                    frame_counters_advance();

        void                    delete_queued_resources( bool force_deletion );
        void                    set_resource_name( VkObjectType type, u64 handle, StringView name );

        TextureHandle           get_current_swapchain_texture();
        void                    create_swapchain();
        void                    destroy_swapchain();

        // TODO: should those be common ?
        // [TAG: BINDLESS]
        void                    create_bindless_resources();
        void                    destroy_bindless_resources();

        DescriptorSetLayoutHandle create_bindless_descriptor_set_layout( const DescriptorSetLayoutCreation& creation );

        void                    fill_buffer_barrier( VkBufferMemoryBarrier2* barrier, BufferHandle buffer, ResourceState::Enum new_state,
                                                     u32 offset, u32 size, u32 source_family = VK_QUEUE_FAMILY_IGNORED,
                                                     u32 destination_family = VK_QUEUE_FAMILY_IGNORED,
                                                     QueueType::Enum source_queue_type = QueueType::Graphics,
                                                     QueueType::Enum destination_queue_type = QueueType::Graphics );

        void                    fill_image_barrier( VkImageMemoryBarrier2* barrier, TextureHandle texture, ResourceState::Enum new_state,
                                                    u32 base_mip_level, u32 mip_count, u32 base_array_layer, u32 array_layer_count, 
                                                    u32 source_family = VK_QUEUE_FAMILY_IGNORED,
                                                    u32 destination_family = VK_QUEUE_FAMILY_IGNORED,
                                                    QueueType::Enum source_queue_type = QueueType::Graphics, 
                                                    QueueType::Enum destination_queue_type = QueueType::Graphics );


        // Members ////////////////////////////////////////////////////////

        VkInstance              vk_instance             = VK_NULL_HANDLE;
        VkPhysicalDevice        vk_physical_device      = VK_NULL_HANDLE;
        VkDevice                vk_device               = VK_NULL_HANDLE;
        VkSurfaceKHR            vk_window_surface       = VK_NULL_HANDLE;
        VkDescriptorPool        vk_descriptor_pool      = VK_NULL_HANDLE;
        VkDescriptorPool        vk_bindless_descriptor_pool = VK_NULL_HANDLE; // [TAG: BINDLESS]
        
        VkQueue                 vk_queues[ QueueType::Count ];
        u32                     queue_indices[ QueueType::Count ];

        // Swapchain data
        VkFormat                vk_swapchain_format     = VK_FORMAT_UNDEFINED;
        VkSwapchainKHR          vk_swapchain            = VK_NULL_HANDLE;
        VkImage                 vk_swapchain_images[ k_max_swapchain_images ];
        u32                     swapchain_width         = 0;
        u32                     swapchain_height        = 0;
        u32                     swapchain_image_count   = 0;
        u32                     swapchain_image_index   = 0;

        // Per frame synchronization
        VkSemaphore             vk_render_complete_semaphore[ k_max_frames ];
        VkSemaphore             vk_image_acquired_semaphore[ k_max_frames ];

        VkSemaphore             vk_graphics_timeline_semaphore;
        VkSemaphore             vk_compute_timeline_semaphore;
        VkSemaphore             vk_transfer_timeline_semaphore;

        u64                     last_compute_semaphore_value = 0;
        bool                    has_async_work = false;
        u64                     last_transfer_semaphore_value = 0;
        bool                    has_transfer_work = false;

        u32                     previous_frame = 0;
        u32                     current_frame = 0;
        u32                     absolute_frame = 0;

        CommandBuffer*          enqueued_command_buffers[ k_max_enqueued_command_buffers ];
        u32                     num_enqueued_command_buffers = 0;

        Allocator*              allocator   = nullptr;

        // Device informations
        VkPhysicalDeviceProperties vk_physical_device_properties;

        VmaAllocator            vma_allocator               = VK_NULL_HANDLE;
        VkAllocationCallbacks*  vk_allocation_callbacks     = VK_NULL_HANDLE;

        VkDebugUtilsMessengerEXT vk_debug_utils_messenger   = VK_NULL_HANDLE;

        // Resource pools
        Pool<VulkanBuffer, Buffer, BufferHandle> buffers;
        Pool<VulkanTexture, Texture, TextureHandle> textures;
        Pool<VulkanSampler, Sampler, SamplerHandle> samplers;
        Pool<VulkanDescriptorSetLayout, DescriptorSetLayout, DescriptorSetLayoutHandle> descriptor_set_layouts;
        Pool<VulkanDescriptorSet, DescriptorSet, DescriptorSetHandle> descriptor_sets;
        Pool<VulkanPipeline, Pipeline, PipelineHandle> pipelines;
        Pool<VulkanShaderState, ShaderState, ShaderStateHandle> shader_states;

        // Sub resources slots
        SlotAllocator           shader_info_allocators[ PipelineType::Count ];

        SlotAllocator           descriptor_set_bindings_allocators[DescriptorSetBindingsPools::_Count];

        // These are dynamic - so that workload can be handled correctly.
        Array<ResourceUpdate>   resource_deletion_queue;
        //Array<DescriptorSetUpdate>      descriptor_set_updates;
        // [TAG: BINDLESS]
        Array<TextureUpdate>   texture_to_update_bindless;
        Array<UploadTextureData> texture_uploads;
        Array<UploadTextureData> texture_transfer_completes;

        TextureFormat::Enum     swapchain_format;
        u32                     ubo_alignment;
        u32                     ssbo_alignment;
        u32                     max_framebuffer_layers;
        f32                     gpu_timestamp_frequency;
        bool                    bindless_supported              = false;
        bool                    debug_utils_extension_present   = false;

        CommandBufferManager*   command_buffer_manager  = nullptr;

        // Local resources
        SamplerHandle           default_sampler;
        TextureHandle           swapchain_textures[ k_max_swapchain_images ];
        TextureHandle           dummy_texture;

        // [TAG: BINDLESS]
        DescriptorSetLayoutHandle bindless_descriptor_set_layout;
        DescriptorSetHandle     bindless_descriptor_set;

        BufferHandle            staging_buffer;
        u32                     staging_buffer_offset = 0;

        BufferHandle            dynamic_buffer;
        u32                     dynamic_per_frame_size = 0;
        u32                     dynamic_allocated_size = 0;
        u8*                     dynamic_mapped_memory = nullptr;

#endif // IDRA_VULKAN

    }; // GpuDevice

    template<typename T>
    inline T* GpuDevice::dynamic_buffer_allocate( u32* dynamic_offset ) {
        void* allocated_memory = dynamic_buffer_allocate( sizeof( T ), alignof( T ), dynamic_offset );
        return static_cast<T*>( allocated_memory );
    }

} // namespace idra