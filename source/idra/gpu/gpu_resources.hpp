/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string_view.hpp"
#include "kernel/pool.hpp"

#include "gpu/gpu_enums.hpp"

// NOTE: at the end of the file there are the API-specific structs.

namespace idra {


using BufferHandle              = Handle<struct BufferDummy>;
using TextureHandle             = Handle<struct TextureDummy>;
using ShaderStateHandle         = Handle<struct ShaderStateDummy>;
using SamplerHandle             = Handle<struct SamplerDummy>;
using DescriptorSetLayoutHandle = Handle<struct DescriptorSetLayoutDummy>;
using DescriptorSetHandle       = Handle<struct DescriptorSetDummy>;
using PipelineHandle            = Handle<struct PipelineDummy>;

// Consts ///////////////////////////////////////////////////////////////////////

static const u8                     k_max_image_outputs = 8;        // Maximum number of images/render_targets/fbo attachments usable.
static const u8                     k_max_descriptor_set_layouts = 8;        // Maximum number of layouts in the pipeline.
static const u8                     k_max_shader_stages = 5;        // Maximum simultaneous shader stages. Applicable to all different type of pipelines.
static const u8                     k_max_bindings_per_list = 16;       // Maximum list elements for both resource list layout and resource lists.
static const u8                     k_max_vertex_streams = 16;
static const u8                     k_max_vertex_attributes = 16;

static const u32                    k_submit_header_sentinel = 0xfefeb7ba;
static const u32                    k_max_resource_deletions = 64;
static const u32                    k_bindless_count = 1000;
static const u32                    k_max_swapchain_images = 4;
static const u32                    k_max_frames = 2;
static const u32                    k_max_enqueued_command_buffers = 6;

// Resource creation structs ////////////////////////////////////////////////////

//
//
struct Rect2D {
    f32                             x = 0.0f;
    f32                             y = 0.0f;
    f32                             width = 0.0f;
    f32                             height = 0.0f;
}; // struct Rect2D

//
//
struct Rect2DInt {
    i16                             x = 0;
    i16                             y = 0;
    u16                             width = 0;
    u16                             height = 0;
}; // struct Rect2D

//
//
struct ClearColor {

    f32                             rgba[ 4 ];
}; // struct ClearColor

//
//
struct ClearDepthStencil {

    f32                             depth_value;
    u8                              stencil_value;
}; // struct ClearDepthStencil

//
//
struct Viewport {
    Rect2DInt                       rect;
    f32                             min_depth = 0.0f;
    f32                             max_depth = 0.0f;
}; // struct Viewport

//
//
struct ViewportState {

    Span<Viewport>                  viewports;
    Span<Rect2DInt>                 scissors;

}; // struct ViewportState

//
//
struct StencilOperationState {

    StencilOperation::Enum          fail        = StencilOperation::Keep;
    StencilOperation::Enum          pass        = StencilOperation::Keep;
    StencilOperation::Enum          depth_fail  = StencilOperation::Keep;
    ComparisonFunction::Enum        compare     = ComparisonFunction::Always;
    u32                             compare_mask = 0xff;
    u32                             write_mask  = 0xff;
    u32                             reference   = 0xff;

}; // struct StencilOperationState

//
//
struct DepthStencilCreation {

    StencilOperationState           front;
    StencilOperationState           back;
    ComparisonFunction::Enum        depth_comparison    = ComparisonFunction::Always;

    u8                              depth_enable        = 0;
    u8                              depth_write_enable  = 0;
    u8                              stencil_enable      = 0;
}; // struct DepthStencilCreation

struct BlendState {

    Blend::Enum                     source_color        = Blend::One;
    Blend::Enum                     destination_color   = Blend::One;
    BlendOperation::Enum            color_operation     = BlendOperation::Add;

    Blend::Enum                     source_alpha        = Blend::One;
    Blend::Enum                     destination_alpha   = Blend::One;
    BlendOperation::Enum            alpha_operation     = BlendOperation::Add;

    ColorWriteEnabled::Mask         color_write_mask    = ColorWriteEnabled::All_mask;

    u8                              blend_disabled      = 0;
    u8                              separate_blend      = 0;

}; // struct BlendState

struct BlendStateCreation {

    Span<const BlendState>          blend_states;

}; // BlendStateCreation

//
//
struct RasterizationCreation {

    CullMode::Enum                  cull_mode   = CullMode::None;
    FrontClockwise::Enum            front       = FrontClockwise::False;
    FillMode::Enum                  fill        = FillMode::Solid;
}; // struct RasterizationCreation

//
//
struct BufferCreation {

    BufferUsage::Mask               type            = BufferUsage::Count_mask;
    ResourceUsageType::Enum         usage           = ResourceUsageType::Immutable;
    u32                             size            = 0;
    u32                             persistent      = 0;
    u32                             device_only     = 0;
    void*                           initial_data    = nullptr;

    StringView                      debug_name;

}; // struct BufferCreation

//
//
struct TextureCreation {

    u16                             width           = 1;
    u16                             height          = 1;
    u16                             depth           = 1;
    u16                             array_layer_count = 1;
    u8                              mip_level_count = 1;
    u8                              flags           = 0;    // TextureFlags bitmasks

    TextureFormat::Enum             format          = TextureFormat::UNKNOWN;
    TextureType::Enum               type            = TextureType::Texture2D;

    TextureHandle                   alias;
    SamplerHandle                   sampler;
    void*                           initial_data    = nullptr;

    StringView                      debug_name;

}; // struct TextureCreation

//
//
struct TextureSubResource {

    u16                             mip_base_level = 0;
    u16                             mip_level_count = 1;
    u16                             array_base_layer = 0;
    u16                             array_layer_count = 1;

}; // struct TextureSubResource

//
//
struct TextureViewCreation {

    TextureHandle                   parent_texture;

    TextureType::Enum               view_type = TextureType::Texture1D;
    TextureSubResource              sub_resource;

    StringView                      debug_name;

}; // struct TextureViewCreation


//
//
struct SamplerCreation {

    TextureFilter::Enum             min_filter = TextureFilter::Nearest;
    TextureFilter::Enum             mag_filter = TextureFilter::Nearest;
    SamplerMipmapMode::Enum         mip_filter = SamplerMipmapMode::Nearest;

    SamplerAddressMode::Enum        address_mode_u = SamplerAddressMode::Repeat;
    SamplerAddressMode::Enum        address_mode_v = SamplerAddressMode::Repeat;
    SamplerAddressMode::Enum        address_mode_w = SamplerAddressMode::Repeat;

    // VkSamplerReductionMode          reduction_mode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

    StringView                      debug_name;

}; // struct SamplerCreation

struct ShaderStageCode {

    Span<u32>                       byte_code;
    ShaderStage::Enum               type        = ShaderStage::Count;

}; // struct ShaderStageCode

//
//
struct GraphicsShaderStateCreation {

    ShaderStageCode                 vertex_shader;
    ShaderStageCode                 fragment_shader;

    StringView                      debug_name;

}; // struct GraphicsShaderStateCreation

//
//
struct ComputeShaderStateCreation {

    ShaderStageCode                 compute_shader;

    StringView                      debug_name;

}; // struct ComputeShaderStateCreation

//
// A single resource binding. It can be relative to one or more resources of the same type.
//
struct DescriptorBinding {

    DescriptorType::Enum            type = DescriptorType::Count;
    u16                             start = 0;
    u16                             count = 0;
    StringView                      name;
}; // struct Binding

//
//
struct DescriptorSetLayoutCreation {

    Span<const DescriptorBinding>   bindings;
    Span<const u32>                 dynamic_buffer_bindings;

    StringView                      debug_name;

}; // struct DescriptorSetLayoutCreation

//
struct TextureDescriptor {
    TextureHandle                   texture;
    u16                             binding;
};

//
struct BufferDescriptor {
    BufferHandle                    buffer;
    u16                             binding;
};

//
struct SamplerDescriptor {
    SamplerHandle                   sampler;
    u16                             binding;
};

//
struct DynamicBufferBinding {
    u32                             binding;
    u32                             size;
};

//
//
struct DescriptorSetCreation {

    Span<const TextureDescriptor>   textures;
    Span<const TextureDescriptor>   images;
    Span<const BufferDescriptor>    buffers;
    Span<const BufferDescriptor>    ssbos;
    Span<const SamplerDescriptor>   samplers;
    Span<const DynamicBufferBinding> dynamic_buffer_bindings;

    DescriptorSetLayoutHandle       layout;

    StringView                      debug_name;

}; // struct ResourceListCreation

//
//
struct DescriptorSetUpdate {
    //DescriptorSetHandle           descriptor_set;

    u32                             frame_issued = 0;
}; // DescriptorSetUpdate

//
//
struct VertexAttribute {

    u16                             location = 0;
    u16                             binding = 0;
    u32                             offset = 0;
    VertexComponentFormat::Enum     format = VertexComponentFormat::Count;

}; // struct VertexAttribute

//
//
struct VertexStream {

    u16                             binding = 0;
    u16                             stride = 0;
    VertexInputRate::Enum           input_rate = VertexInputRate::Count;

}; // struct VertexStream

//
//
struct VertexInputCreation {

    Span<const VertexStream>        vertex_streams;
    Span<const VertexAttribute>     vertex_attributes;
}; // struct VertexInputCreation

//
//
struct RenderPassOutput {

    Span<const TextureFormat::Enum> color_formats;
    TextureFormat::Enum             depth_stencil_format;

    LoadOperation::Enum       color_operation         = LoadOperation::DontCare;
    LoadOperation::Enum       depth_operation         = LoadOperation::DontCare;
    LoadOperation::Enum       stencil_operation       = LoadOperation::DontCare;

}; // struct RenderPassOutput

//
//
struct RenderPassCreation {

    //RenderPassType::Enum            type                = RenderPassType::Geometry;

    Span<const TextureHandle>       output_textures;
    TextureHandle                   depth_stencil_texture;

    f32                             scale_x             = 1.f;
    f32                             scale_y             = 1.f;
    u8                              resize              = 1;

    LoadOperation::Enum       color_operation         = LoadOperation::DontCare;
    LoadOperation::Enum       depth_operation         = LoadOperation::DontCare;
    LoadOperation::Enum       stencil_operation       = LoadOperation::DontCare;

    StringView                      debug_name;

}; // struct RenderPassCreation

//
//
struct GraphicsPipelineCreation {

    RasterizationCreation           rasterization;
    DepthStencilCreation            depth_stencil;
    BlendStateCreation              blend_state;
    VertexInputCreation             vertex_input;
    ShaderStateHandle               shader;

    //RenderPassOutput                render_pass;
    Span<const DescriptorSetLayoutHandle> descriptor_set_layouts;
    ViewportState                   viewport;

    // Output
    Span<const TextureFormat::Enum> color_formats;
    TextureFormat::Enum             depth_format    = TextureFormat::UNKNOWN;

    StringView                      debug_name;

}; // struct PipelineCreation

//
//
struct ComputePipelineCreation {

    //ComputeShaderStateCreation      shader_state;
    ShaderStateHandle               shader;

    Span<const DescriptorSetLayoutHandle> descriptor_set_layouts;

    StringView                      debug_name;

}; // struct ComputePipelineCreation

//
//
struct ResourceUpdate {

    Handle<struct ResourceDummy>    handle; // cloning index and generation and back
    u32                             current_frame;
    ResourceUpdateType::Enum        type;

}; // struct ResourceUpdate

//
//
struct TextureUpdate {
    TextureHandle                   texture;
    u32                             current_frame;
    u8                              deleting;
};

//
//
struct UploadTextureData {
    TextureHandle                   texture;
    void*                           data;
}; // struct UploadTextureData

//
//
struct TextureBarrier {
    TextureHandle                   texture;
    ResourceState::Enum             new_state;
    u32                             mip_level;
    u32                             mip_count;
    QueueType::Enum                 source_queue = QueueType::Graphics;
    QueueType::Enum                 destination_queue = QueueType::Graphics;
};

//
//
struct BufferBarrier {
    BufferHandle                    buffer;
    ResourceState::Enum             new_state;
    u32                             offset = 0;
    u32                             size = 0;
};

} // namespace idra

#if defined (IDRA_VULKAN)

namespace idra {

    // Vulkan resources ///////////////////////////////////////////////////
    //
    struct Buffer {
        VmaAllocation                   vma_allocation;
        VkDeviceMemory                  vk_device_memory;
        VkDeviceSize                    vk_device_size;

        VkBufferUsageFlags              type_flags      = 0;
        ResourceUsageType::Enum         usage           = ResourceUsageType::Immutable;
        u32                             size            = 0;
        ResourceState::Enum             state           = ResourceState::Undefined;

        BufferHandle                    handle;

        bool                            ready           = true;

        u8*                             mapped_data     = nullptr;
        StringView                      name;
    }; // struct Buffer


    //
    struct VulkanBuffer {
        VkBuffer                        vk_buffer;
    };

    //
    struct Sampler {

        // TODO: do I need the Vulkan specific or the generic ones ?
        VkOpaqueEnum                    min_filter; // VK_FILTER_NEAREST;
        VkOpaqueEnum                    mag_filter; // VK_FILTER_NEAREST;
        VkOpaqueEnum                    mip_filter; // VK_SAMPLER_MIPMAP_MODE_NEAREST;

        VkOpaqueEnum                    address_mode_u; //VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkOpaqueEnum                    address_mode_v; //VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkOpaqueEnum                    address_mode_w; // VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VkOpaqueEnum                    reduction_mode; // VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

        StringView                      name;
    };

    struct VulkanSampler {
        VkSampler                       vk_sampler;
    };

    //
    //
    struct Texture {
        
        VkOpaqueEnum                    vk_format;
        TextureFormat::Enum             format;
        VkImageUsageFlags               vk_usage;
        VmaAllocation                   vma_allocation  = nullptr;

        u16                             width           = 1;
        u16                             height          = 1;
        u16                             depth           = 1;
        u16                             array_layer_count = 1;
        u8                              mip_level_count = 1;
        u8                              flags           = 0;
        u16                             mip_base_level  = 0;    // Not 0 when texture is a view.
        u16                             array_base_layer = 0;   // Not 0 when texture is a view.
        bool                            sparse = false;

        TextureHandle                   handle;
        TextureHandle                   parent_texture;     // Used when a texture view.
        TextureHandle                   alias_texture;
        TextureType::Enum               type    = TextureType::Texture2D;

        SamplerHandle                   sampler;

        StringView                      name;
    };

    struct VulkanTexture {

        VkImage                         vk_image;
        VkImageView                     vk_image_view;

        ResourceState::Enum             state = ResourceState::Undefined;
    };


    //
    //
    struct DescriptorSetLayout {

        VkDescriptorSetLayoutBinding*   vk_binding = nullptr;
        u16                             num_bindings = 0;
        u16                             num_dynamic_bindings = 0;
        u8                              bindless = 0;
        u8                              dynamic = 0;

        DescriptorSetLayoutHandle       handle;
        StringView                      name;
    };

    struct VulkanDescriptorSetLayout {
        VkDescriptorSetLayout           vk_descriptor_set_layout;
    };

    //
    //
    struct DescriptorSet {

        VkAccelerationStructureKHR      as              = nullptr;

        const DescriptorSetLayout*      layout          = nullptr;
        StringView                      name;
    };

    struct VulkanDescriptorSet {
        VkDescriptorSet                 vk_descriptor_set;
    };

    //
    //
    struct Pipeline {

        ShaderStateHandle               shader_state;

        const DescriptorSetLayout*      descriptor_set_layout[ k_max_descriptor_set_layouts ];
        DescriptorSetLayoutHandle       descriptor_set_layout_handles[ k_max_descriptor_set_layouts ];
        u32                             num_active_layouts = 0;

        DepthStencilCreation            depth_stencil;
        BlendStateCreation              blend_state;
        RasterizationCreation           rasterization;

        BufferHandle                    shader_binding_table_raygen;
        BufferHandle                    shader_binding_table_hit;
        BufferHandle                    shader_binding_table_miss;

        PipelineType::Enum              pipeline_type   = PipelineType::Count;
    }; // struct Pipeline

    struct VulkanPipeline {

        VkPipeline                      vk_pipeline;
        VkPipelineLayout                vk_pipeline_layout;

        VkOpaqueEnum                    vk_bind_point;

    }; // struct VulkanPipeline


    //
    //
    struct ShaderState {

        VkPipelineShaderStageCreateInfo*    shader_stage_info   = nullptr;
        VkRayTracingShaderGroupCreateInfoKHR*  shader_group_info = nullptr;

        StringView                      debug_name;

        u32                             num_active_shaders      = 0;
        PipelineType::Enum              pipeline_type           = PipelineType::Count;

    }; // struct ShaderState

    //
    struct VulkanShaderState {
        // Vulkan does not use directly a shader state,
        // so no need for any data here.
    }; // struct VulkanShaderState


    ///////////////////////////////////////////////////////////////////////
    // Utility methods

    namespace GpuUtils {
        size_t                          calculate_texture_size( Texture* texture );
    } // namespace GpuUtils

} // namespace idra

#endif // IDRA_VULKAN
