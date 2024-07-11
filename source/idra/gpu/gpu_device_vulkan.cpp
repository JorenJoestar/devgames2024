/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "gpu_device.hpp"
#include "command_buffer.hpp"

#include "kernel/assert.hpp"
#include "kernel/file.hpp"

#include <vulkan/vk_enum_string_helper.h>

//#define VMA_MAX idra_max
//#define VMA_MIN idra_min
#define VMA_USE_STL_CONTAINERS 0
#define VMA_USE_STL_VECTOR 0
#define VMA_USE_STL_UNORDERED_MAP 0
#define VMA_USE_STL_LIST 0

#define VMA_IMPLEMENTATION
#include "external/vk_mem_alloc.h"

// Shader compiler DLL interface
#include "../tools/shader_compiler/shader_compiler.hpp"

// SDL and Vulkan headers
#include <SDL.h>
#include <SDL_vulkan.h>


#if defined(_MSC_VER)

#else
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>

// NOTE(marco): avoid conflicts with X header...
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif
#ifdef Always
#undef Always
#endif
#ifdef None
#undef None
#endif

#endif // _MSC_VER


namespace idra {

// Enum translations
static VkAccessFlags            to_vk_access_flags2( ResourceState::Enum state );
static VkSamplerAddressMode     to_vk_address_mode( SamplerAddressMode::Enum value );
static VkBlendFactor            to_vk_blend_factor( Blend::Enum value );
static VkBlendOp                to_vk_blend_operation( BlendOperation::Enum value );
static VkCompareOp              to_vk_compare_operation( ComparisonFunction::Enum value );
static VkCullModeFlags          to_vk_cull_mode( CullMode::Enum value );
static VkDescriptorType         to_vk_descriptor_type( DescriptorType::Enum type );
static VkFilter                 to_vk_filter( TextureFilter::Enum value );
static VkFormat                 to_vk_format( TextureFormat::Enum format );
static VkFrontFace              to_vk_front_face( FrontClockwise::Enum value );
static VkImageType              to_vk_image_type( TextureType::Enum type );
static VkImageViewType          to_vk_image_view_type( TextureType::Enum type );
static VkImageLayout            to_vk_image_layout2( ResourceState::Enum usage );
static VkIndexType              to_vk_index_type( IndexType::Enum type );
static VkSamplerMipmapMode      to_vk_mipmap( SamplerMipmapMode::Enum value );
static VkPipelineStageFlags     to_vk_pipeline_stage( PipelineStage::Enum value );
static VkPolygonMode            to_vk_polygon_mode( FillMode::Enum value );
static VkShaderStageFlagBits    to_vk_shader_stage( ShaderStage::Enum value );
static VkFormat                 to_vk_vertex_format( VertexComponentFormat::Enum value );


// Checkpoint enum ////////////////////////////////////////////////////////
namespace GpuDeviceCheckpoint {

    enum Enum {
        Uninitialized = 0,
        VolkInitialized,
        InstanceCreated,
        DebugReportCreated,
        PhysicalDeviceFound,
        LogicalDeviceCreated,
        SwapchainSurfaceCreated,
        SwapchainCreated,
        VMAAllocatorCreated,
        Initialized
    }; // enum Enum

    static cstring EnumNames[] = {

        "Uninitialized",
        "VolkInitialized",
        "InstanceCreated",
        "DebugReportCreated",
        "PhysicalDeviceFound",
        "LogicalDeviceCreated",
        "SwapchainSurfaceCreated",
        "SwapchainCreated",
        "VMAAllocatorCreated",
        "Initialized"
    };

    static void handle_error( GpuDevice& gpu, GpuDeviceCheckpoint::Enum checkpoint ) {

        ilog_error( "GpuDevice: error in checkpoint %s, system cannot be created.\n", EnumNames[ checkpoint ] );

        switch ( checkpoint ) {
            case idra::GpuDeviceCheckpoint::VMAAllocatorCreated:

                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::SwapchainCreated:

                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::SwapchainSurfaceCreated:

                vkDestroySurfaceKHR( gpu.vk_instance, gpu.vk_window_surface, gpu.vk_allocation_callbacks );
                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::LogicalDeviceCreated:

                vkDestroyDevice( gpu.vk_device, gpu.vk_allocation_callbacks );
                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::PhysicalDeviceFound:

                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::DebugReportCreated:

                if ( gpu.vk_debug_utils_messenger != VK_NULL_HANDLE ) {
                    vkDestroyDebugUtilsMessengerEXT( gpu.vk_instance, gpu.vk_debug_utils_messenger, gpu.vk_allocation_callbacks );
                }
                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::InstanceCreated:

                vkDestroyInstance( gpu.vk_instance, gpu.vk_allocation_callbacks );
                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::VolkInitialized:

                volkFinalize();
                [[fallthrough]];

            case idra::GpuDeviceCheckpoint::Uninitialized:
                break;

            case idra::GpuDeviceCheckpoint::Initialized:
                break;

            default:
                ilog_error( "Error in checkpoint value %u\n", checkpoint );
                break;
        }
    }

} // namespace GpuDeviceCheckpoint

static SDL_Window* sdl_window;
static GpuDevice s_gpu_device_vulkan;

// TODO:
GpuDeviceCheckpoint::Enum s_current_checkpoint;

// System init/shutdown ///////////////////////////////////////////////////
GpuDevice* GpuDevice::init_system( const GpuDeviceCreation& creation ) {

    const bool init_success = s_gpu_device_vulkan.internal_init( creation );
    // See if the system is successfully created otherwise return a nullptr.
    if ( init_success ) {
        return &s_gpu_device_vulkan;
    }
    else {
        ilog_error( "Error initializing GPUDevice, %u\n", s_current_checkpoint );
        GpuDeviceCheckpoint::handle_error( s_gpu_device_vulkan, s_current_checkpoint );

        return nullptr;
    }
}

void GpuDevice::shutdown_system( GpuDevice* instance ) {
    iassert( instance == &s_gpu_device_vulkan );

    s_gpu_device_vulkan.internal_shutdown();
}

// Vulkan options /////////////////////////////////////////////////////////

// Enable this to add debugging capabilities.
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_utils.html

#if defined (_DEBUG)
#define VULKAN_DEBUG_REPORT
#endif // _DEBUG

// Enabling this slows down the runtime by a lot.
//#define VULKAN_SYNCHRONIZATION_VALIDATION

// Features
namespace GpuFeatures {
    enum Enum {
        DynamicRendering,
        Synchronization2,
        TimelineSemaphore,
        MemoryBudget,
        Count
    };

    static cstring s_names[ Count ] = {
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    };

    static bool s_supported[ Count ];
} // namespace GpuFeatures

#define         vkcheck( result ) iassertm( result == VK_SUCCESS, "Vulkan assert code %u, '%s'", result, string_VkResult( result ) )
#define         vkcheckpoint( function ) { VkResult result = function; if ( result != VK_SUCCESS) { ilog("Vulkan assert code %u, '%s'\n", result, string_VkResult( result ));return false;} }

static VkBool32 debug_utils_callback( VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                      VkDebugUtilsMessageTypeFlagsEXT types,
                                      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                      void* user_data ) {
    ilog( " MessageID: %s %i\nMessage: %s\n\n", callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage );
    return VK_FALSE;
}


static VkPipelineStageFlags2KHR util_determine_pipeline_stage_flags2( VkAccessFlags2KHR access_flags, QueueType::Enum queue_type );

// Utility methods ////////////////////////////////////////////////////////
static void util_fill_image_barrier(
                VkImageMemoryBarrier2* barrier, VkImage image, ResourceState::Enum old_state,
                ResourceState::Enum new_state, u32 base_mip_level, u32 mip_count, u32 base_array_layer,
                u32 array_layer_count, bool is_depth, u32 source_family, u32 destination_family,
                QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type );

static void util_fill_buffer_barrier(
                VkBufferMemoryBarrier2KHR* barrier, VkBuffer buffer,
                ResourceState::Enum old_state, ResourceState::Enum new_state,
                u32 offset, u32 size, u32 source_family, u32 destination_family,
                QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type );


static DescriptorSetBindingsPools::Enum get_binding_allocator_index( u32 num_bindings );

static const u32        k_bindless_texture_binding = 10;
static const u32        k_bindless_image_binding = 11;
static const u32        k_max_bindless_resources = 1024;

// GpuDevice //////////////////////////////////////////////////////////////
bool GpuDevice::internal_init( const GpuDeviceCreation& creation ) {

    ilog( "gpu device vulkan init!\n" );

    const u32 vulkan_api_version = VK_API_VERSION_1_3;

    // Init volk and check support for Vulkan 1.3.
    vkcheck( volkInitialize() );
    iassert( volkGetInstanceVersion() >= vulkan_api_version );
    s_current_checkpoint = GpuDeviceCheckpoint::VolkInitialized;

    // Instance creation //////////////////////////////////////////////////
    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Idra",
        .applicationVersion = 1,
        .pEngineName = "Idra",
        .engineVersion = VK_MAKE_API_VERSION( 0, 0, 4, 0 ),
        .apiVersion = vulkan_api_version
    };

    // Instance extensions
    cstring instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#ifdef _DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    // Layers
    cstring instance_layers[] = {
#if defined (VULKAN_DEBUG_REPORT)
        "VK_LAYER_KHRONOS_validation",
#else
        "",
#endif // VULKAN_DEBUG_REPORT
    };

#if defined (VULKAN_DEBUG_REPORT)
    const u32 instance_layers_count = ArraySize( instance_layers );

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debug_messenger_create_info.pfnUserCallback = debug_utils_callback;
    debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

#else
    const u32 instance_layers_count = ArraySize( instance_layers ) - 1;
#endif // VULKAN_DEBUG_REPORT

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &application_info,
        .enabledLayerCount = instance_layers_count,
        .ppEnabledLayerNames = instance_layers,
        .enabledExtensionCount = ArraySize( instance_extensions ),
        .ppEnabledExtensionNames = instance_extensions
    };

#if defined(VULKAN_DEBUG_REPORT)
#if defined(VULKAN_SYNCHRONIZATION_VALIDATION)
    const VkValidationFeatureEnableEXT featuresRequested[] = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT/*, VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT*/ };
    VkValidationFeaturesEXT features = {};
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.pNext = &debug_messenger_create_info;
    features.enabledValidationFeatureCount = _countof( featuresRequested );
    features.pEnabledValidationFeatures = featuresRequested;

    create_info.pNext = &features;
#else
    create_info.pNext = &debug_messenger_create_info;
#endif // VULKAN_SYNCHRONIZATION_VALIDATION
#endif // VULKAN_DEBUG_REPORT

    vkcheckpoint( vkCreateInstance( &create_info, vk_allocation_callbacks, &vk_instance ) );

    s_current_checkpoint = GpuDeviceCheckpoint::InstanceCreated;
    volkLoadInstanceOnly( vk_instance );

    // Debug utils extension //////////////////////////////////////////////
    debug_utils_extension_present = false;

    BookmarkAllocator* temp_allocator = g_memory->get_thread_allocator();
    sizet current_marker = temp_allocator->get_marker();

#ifdef VULKAN_DEBUG_REPORT
    {
        u32 num_instance_extensions;
        vkEnumerateInstanceExtensionProperties( nullptr, &num_instance_extensions, nullptr );
        VkExtensionProperties* extensions = ( VkExtensionProperties* )ialloc( sizeof( VkExtensionProperties ) * num_instance_extensions, temp_allocator );
        vkEnumerateInstanceExtensionProperties( nullptr, &num_instance_extensions, extensions );
        for ( size_t i = 0; i < num_instance_extensions; i++ ) {

            if ( !strcmp( extensions[ i ].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) ) {
                debug_utils_extension_present = true;
                break;
            }
        }

        if ( debug_utils_extension_present ) {
            vkcheck( vkCreateDebugUtilsMessengerEXT( vk_instance, &debug_messenger_create_info, vk_allocation_callbacks, &vk_debug_utils_messenger ) );
        } else {
            ilog_warn( "Extension %s for debugging non present.", VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        }
    }
#endif

    temp_allocator->free_marker( current_marker );

    // Physical device selection //////////////////////////////////////////
    u32 num_physical_device;
    vkcheck( vkEnumeratePhysicalDevices( vk_instance, &num_physical_device, NULL ) );

    VkPhysicalDevice gpus[ 16 ];
    vkcheck( vkEnumeratePhysicalDevices( vk_instance, &num_physical_device, gpus ) );

    VkPhysicalDevice discrete_gpu = VK_NULL_HANDLE;
    VkPhysicalDevice integrated_gpu = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties vk_physical_properties;

    for ( u32 i = 0; i < num_physical_device; ++i ) {
        VkPhysicalDevice physical_device = gpus[ i ];
        vkGetPhysicalDeviceProperties( physical_device, &vk_physical_properties );

        if ( vk_physical_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
            discrete_gpu = physical_device;
            continue;
        }

        if ( vk_physical_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
            integrated_gpu = physical_device;
            continue;
        }
    }

    if ( discrete_gpu != VK_NULL_HANDLE ) {
        vk_physical_device = discrete_gpu;
    } else if ( integrated_gpu != VK_NULL_HANDLE ) {
        vk_physical_device = integrated_gpu;
    } else {
        iassertm( false, "Suitable GPU device not found!" );
        return false;
    }

    s_current_checkpoint = GpuDeviceCheckpoint::PhysicalDeviceFound;

    // Cache chosen GPU physical properties
    vkGetPhysicalDeviceProperties( vk_physical_device, &vk_physical_device_properties );

    ilog( "GPU Used: %s\n", vk_physical_device_properties.deviceName );
    gpu_timestamp_frequency = vk_physical_device_properties.limits.timestampPeriod / ( 1000 * 1000 );

    // Reset gpu features support array
    memset( GpuFeatures::s_supported, 0, sizeof( GpuFeatures::s_supported ) * sizeof( bool ) );

    // Check features supported by the chosen GPU. ////////////////////////
    u32 num_device_extensions = 0;
    vkEnumerateDeviceExtensionProperties( vk_physical_device, nullptr, &num_device_extensions, nullptr );

    VkExtensionProperties* extensions = ( VkExtensionProperties* )ialloc( sizeof( VkExtensionProperties ) * num_device_extensions, temp_allocator );
    vkEnumerateDeviceExtensionProperties( vk_physical_device, nullptr, &num_device_extensions, extensions );
    for ( size_t i = 0; i < num_device_extensions; i++ ) {

        for ( u32 f = 0; f < GpuFeatures::Count; ++f ) {

            if ( strcmp( extensions[ i ].extensionName, GpuFeatures::s_names[ f ] ) == 0 ) {
                GpuFeatures::s_supported[ f ] = 1;
                break;
            }
        }
    }

    // Log enabled extensions
    ilog_debug( "Enabled device extensions:\n" );
    for ( u32 f = 0; f < GpuFeatures::Count; ++f ) {
        if ( GpuFeatures::s_supported[ f ] ) {
            ilog_debug( "%s\n", GpuFeatures::s_names[ f ] );
        }
    }
    ilog_debug( "\n" );

    temp_allocator->free_marker( current_marker );

    VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2( vk_physical_device, &properties );

    ubo_alignment = (u32)properties.properties.limits.minUniformBufferOffsetAlignment;
    ssbo_alignment = (u32)properties.properties.limits.minStorageBufferOffsetAlignment;
    max_framebuffer_layers = properties.properties.limits.maxFramebufferLayers;

    // Queues support /////////////////////////////////////////////////////
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( vk_physical_device, &queue_family_count, nullptr );

    VkQueueFamilyProperties* queue_families = ( VkQueueFamilyProperties* )ialloc( sizeof( VkQueueFamilyProperties ) * queue_family_count, temp_allocator );
    vkGetPhysicalDeviceQueueFamilyProperties( vk_physical_device, &queue_family_count, queue_families );

    u32 main_queue_family_index = u32_max, transfer_queue_family_index = u32_max, compute_queue_family_index = u32_max, present_queue_family_index = u32_max;
    u32 compute_queue_index = u32_max;
    for ( u32 fi = 0; fi < queue_family_count; ++fi ) {
        VkQueueFamilyProperties queue_family = queue_families[ fi ];

        if ( queue_family.queueCount == 0 ) {
            continue;
        }
#if defined(_DEBUG)
        ilog( "Family %u, flags %u queue count %u\n", fi, queue_family.queueFlags, queue_family.queueCount );
#endif // DEBUG

        // Search for main queue that should be able to do all work (graphics, compute and transfer)
        if ( ( queue_family.queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ) ) == ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ) ) {
            main_queue_family_index = fi;

            // TODO: bring back support for sparse binding
            //iassert( ( queue_family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ) == VK_QUEUE_SPARSE_BINDING_BIT );
            // TODO: why we had this ?
            /*if ( queue_family.queueCount > 1 ) {
                compute_queue_family_index = fi;
                compute_queue_index = 1;
            }*/

            continue;
        }

        // Search for another compute queue if graphics queue exposes only one queue
        if ( ( queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT ) && compute_queue_index == u32_max ) {
            compute_queue_family_index = fi;
            compute_queue_index = 0;
        }

        // Search for transfer queue
        if ( ( queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT ) == 0 && ( queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT ) ) {
            transfer_queue_family_index = fi;
            continue;
        }
    }

    // Set transfer queue on main queue if not supported by GPU
    transfer_queue_family_index = transfer_queue_family_index ==  u32_max ? main_queue_family_index : transfer_queue_family_index;

    // Cache family indices
    queue_indices[ QueueType::Graphics ] = main_queue_family_index;
    queue_indices[ QueueType::Compute ] = compute_queue_family_index;
    queue_indices[ QueueType::Transfer ] = transfer_queue_family_index;

    temp_allocator->free_marker( current_marker );

    const float queue_priority[] = { 1.f, 1.f, 1.f };
    VkDeviceQueueCreateInfo queue_info[ 3 ] = {};

    u32 queue_count = 0;

    VkDeviceQueueCreateInfo& main_queue = queue_info[ queue_count++ ];
    main_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    main_queue.queueFamilyIndex = main_queue_family_index;
    main_queue.queueCount = 1;
    // TODO: old behaviour
    //main_queue.queueCount = compute_queue_family_index == main_queue_family_index ? 2 : 1;
    main_queue.pQueuePriorities = queue_priority;

    if ( compute_queue_family_index != main_queue_family_index ) {
        VkDeviceQueueCreateInfo& compute_queue = queue_info[ queue_count++ ];
        compute_queue.queueFamilyIndex = compute_queue_family_index;
        compute_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        compute_queue.queueCount = 1;
        compute_queue.pQueuePriorities = queue_priority;
    }

    if ( transfer_queue_family_index < queue_family_count ) {
        VkDeviceQueueCreateInfo& transfer_queue_info = queue_info[ queue_count++ ];
        transfer_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        transfer_queue_info.queueFamilyIndex = transfer_queue_family_index;
        transfer_queue_info.queueCount = 1;
        transfer_queue_info.pQueuePriorities = queue_priority;
    }

    // Add extensions to load
    Array<cstring> enabled_extensions;
    enabled_extensions.init( temp_allocator, GpuFeatures::Count + 2, 0 );

    enabled_extensions.push( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
    enabled_extensions.push( VK_EXT_MEMORY_BUDGET_EXTENSION_NAME );

    for ( u32 f = 0; f < GpuFeatures::Count; ++f ) {

        if ( GpuFeatures::s_supported[ f ] ) {
            ilog_debug( "Enabling extension %s\n", GpuFeatures::s_names[ f ] );
            enabled_extensions.push( GpuFeatures::s_names[ f ] );
        }
    }

    // Enable all features: just pass the physical features 2 struct.
    VkPhysicalDeviceFeatures2 physical_features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceVulkan11Features vk11_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    VkPhysicalDeviceVulkan12Features vk12_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan13Features vk13_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

    physical_features2.pNext = &vk11_features;
    vk11_features.pNext = &vk12_features;
    vk12_features.pNext = &vk13_features;

    vkGetPhysicalDeviceFeatures2( vk_physical_device, &physical_features2 );

    bindless_supported = vk12_features.descriptorBindingPartiallyBound&& vk12_features.runtimeDescriptorArray;

    // Logical device creation ////////////////////////////////////////////

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physical_features2,
        .flags = 0,
        .queueCreateInfoCount = queue_count,
        .pQueueCreateInfos = queue_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = enabled_extensions.size,
        .ppEnabledExtensionNames = enabled_extensions.data };

    vkcheck( vkCreateDevice( vk_physical_device, &device_create_info, vk_allocation_callbacks, &vk_device ) );

    volkLoadDevice( vk_device );

    // VMA creation
    const VmaVulkanFunctions funcs = {
      .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
      .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
      .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
      .vkAllocateMemory = vkAllocateMemory,
      .vkFreeMemory = vkFreeMemory,
      .vkMapMemory = vkMapMemory,
      .vkUnmapMemory = vkUnmapMemory,
      .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
      .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
      .vkBindBufferMemory = vkBindBufferMemory,
      .vkBindImageMemory = vkBindImageMemory,
      .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
      .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
      .vkCreateBuffer = vkCreateBuffer,
      .vkDestroyBuffer = vkDestroyBuffer,
      .vkCreateImage = vkCreateImage,
      .vkDestroyImage = vkDestroyImage,
      .vkCmdCopyBuffer = vkCmdCopyBuffer,
      .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
      .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
      .vkBindBufferMemory2KHR = vkBindBufferMemory2,
      .vkBindImageMemory2KHR = vkBindImageMemory2,
      .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
      .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
      .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
    };

    const VmaAllocatorCreateInfo ci = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = vk_physical_device,
        .device = vk_device,
        .preferredLargeHeapBlockSize = 0,
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = &funcs,
        .instance = vk_instance,
        .vulkanApiVersion = vulkan_api_version,
    };

    vkcheck( vmaCreateAllocator( &ci, &vma_allocator ) );

    temp_allocator->free_marker( current_marker );

    // Get main queue
    vkGetDeviceQueue( vk_device, main_queue_family_index, 0, &vk_queues[ QueueType::Graphics ] );

    // TODO(marco): handle case where we can't create a separate compute queue
    if ( queue_indices[ QueueType::Compute ] < queue_family_count ) {
        vkGetDeviceQueue( vk_device, compute_queue_family_index, 0 /*compute_queue_index*/, &vk_queues[ QueueType::Compute ] );
    }

    // Get transfer queue if present
    if ( queue_indices[ QueueType::Transfer ] < queue_family_count ) {
        vkGetDeviceQueue( vk_device, transfer_queue_family_index, 0, &vk_queues[ QueueType::Transfer ] );
    }

    //////// Create drawable surface
    // Create surface
    SDL_Window* window = ( SDL_Window* )creation.os_window_handle;
    if ( SDL_Vulkan_CreateSurface( window, vk_instance, &vk_window_surface) == SDL_FALSE ) {
        ilog_error( "Failed to create Vulkan surface.\n" );
    }

    sdl_window = window;

    // Create Framebuffers
    int window_width, window_height;
    SDL_GetWindowSize( window, &window_width, &window_height );
    swapchain_width = window_width;
    swapchain_height = window_height;

    // Select swapchain format
    VkBool32 surface_supported = 0;
    vkcheck( vkGetPhysicalDeviceSurfaceSupportKHR( vk_physical_device, main_queue_family_index, vk_window_surface, &surface_supported ) );
    iassert( surface_supported );

    u32 format_count = 0;
    vkcheck( vkGetPhysicalDeviceSurfaceFormatsKHR( vk_physical_device, vk_window_surface, &format_count, 0 ) );
    iassert( format_count > 0 );

    VkSurfaceFormatKHR* formats = ( VkSurfaceFormatKHR* )ialloc( sizeof( VkSurfaceFormatKHR) * format_count, temp_allocator );
    vkcheck( vkGetPhysicalDeviceSurfaceFormatsKHR( vk_physical_device, vk_window_surface, &format_count, formats ) );

    const VkColorSpaceKHR surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    vk_swapchain_format = VK_FORMAT_UNDEFINED;
    if ( format_count == 1 && formats[ 0 ].format == VK_FORMAT_UNDEFINED ) {
        vk_swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;
    }

    for ( uint32_t i = 0; i < format_count; ++i ) {
        const VkSurfaceFormatKHR& format = formats[ i ];
        if ( ((format.format == VK_FORMAT_R8G8B8A8_UNORM) || (format.format == VK_FORMAT_B8G8R8A8_UNORM)) && (format.colorSpace == surface_color_space) ) {
            vk_swapchain_format = formats[ i ].format;
            break;
        }
    }

    switch ( vk_swapchain_format ) {
        case VK_FORMAT_R8G8B8A8_UNORM:
            swapchain_format = TextureFormat::R8G8B8A8_UNORM;
            break;
        case VK_FORMAT_B8G8R8A8_UNORM:
            swapchain_format = TextureFormat::B8G8R8A8_UNORM;
            break;
    }

    temp_allocator->free_marker( current_marker );

    //  Create Descriptor Pools
    const GpuDescriptorPoolCreation& pool_creation = creation.descriptor_pool_creation;
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, pool_creation.samplers },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pool_creation.combined_image_samplers },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pool_creation.sampled_image },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, pool_creation.storage_image },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, pool_creation.uniform_texel_buffers },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, pool_creation.storage_texel_buffers },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, pool_creation.uniform_buffer },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, pool_creation.storage_buffer },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, pool_creation.uniform_buffer_dynamic },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, pool_creation.storage_buffer_dynamic },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, pool_creation.input_attachments }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    // TODO:
    pool_info.maxSets = 4096;
    pool_info.poolSizeCount = ( u32 )ArraySize( pool_sizes );
    pool_info.pPoolSizes = pool_sizes;
    vkcheck( vkCreateDescriptorPool( vk_device, &pool_info, vk_allocation_callbacks, &vk_descriptor_pool ) );

    // TODO: memory management ?
    allocator = creation.system_allocator;

    // Create resource pools //////////////////////////////////////////////
    buffers.init( creation.system_allocator, creation.resource_pool_creation.buffers );
    shader_states.init( creation.system_allocator, creation.resource_pool_creation.shaders );
    descriptor_set_layouts.init( creation.system_allocator, creation.resource_pool_creation.descriptor_set_layouts );
    descriptor_sets.init( creation.system_allocator, creation.resource_pool_creation.descriptor_sets );
    pipelines.init( creation.system_allocator, creation.resource_pool_creation.pipelines );
    textures.init( creation.system_allocator, creation.resource_pool_creation.textures );
    samplers.init( creation.system_allocator, creation.resource_pool_creation.samplers );

    // Create sub-resources allocators ////////////////////////////////////
    shader_info_allocators[ PipelineType::Graphics ].init( allocator,
                                                           creation.resource_pool_creation.graphics_shader_info,
                                                           sizeof( VkPipelineShaderStageCreateInfo ) * 2,
                                                           "VkPipelineShaderStageCreateInfo for Graphics" );
    shader_info_allocators[ PipelineType::Compute ].init( allocator,
                                                           creation.resource_pool_creation.compute_shader_info,
                                                           sizeof( VkPipelineShaderStageCreateInfo ),
                                                          "VkPipelineShaderStageCreateInfo for Compute" );
    shader_info_allocators[ PipelineType::Raytracing ].init( allocator,
                                                           creation.resource_pool_creation.ray_tracing_shader_info,
                                                           sizeof( VkRayTracingShaderGroupCreateInfoKHR ) * k_max_shader_stages,
                                                             "VkPipelineShaderStageCreateInfo for Ray-Tracing" );

    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_2 ].init( allocator,
                                                                               creation.resource_pool_creation.descriptor_set_bindings_2,
                                                                               sizeof(VkDescriptorSetLayoutBinding) * 2,
                                                                               "VkDescriptorSetLayoutBinding Pool of 2");
    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_4 ].init( allocator,
                                                                               creation.resource_pool_creation.descriptor_set_bindings_4,
                                                                               sizeof( VkDescriptorSetLayoutBinding ) * 4,
                                                                               "VkDescriptorSetLayoutBinding Pool of 4" );

    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_8 ].init( allocator,
                                                                               creation.resource_pool_creation.descriptor_set_bindings_8,
                                                                               sizeof( VkDescriptorSetLayoutBinding ) * 8,
                                                                               "VkDescriptorSetLayoutBinding Pool of 8" );

    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_16 ].init( allocator,
                                                                               creation.resource_pool_creation.descriptor_set_bindings_16,
                                                                               sizeof( VkDescriptorSetLayoutBinding ) * 16,
                                                                                "VkDescriptorSetLayoutBinding Pool of 16" );

    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_32 ].init( allocator,
                                                                               creation.resource_pool_creation.descriptor_set_bindings_32,
                                                                               sizeof( VkDescriptorSetLayoutBinding ) * 32,
                                                                                "VkDescriptorSetLayoutBinding Pool of 32" );

    resource_deletion_queue.init( allocator, 32, 0 );
    texture_uploads.init( allocator, 32, 0 );
    texture_transfer_completes.init( allocator, 32, 0 );
    texture_to_update_bindless.init( allocator, 32, 0 );

    command_buffer_manager = ( CommandBufferManager* )ialloc( sizeof( CommandBufferManager ), allocator );
    *command_buffer_manager = {}; // init to default values
    command_buffer_manager->init( this, creation.resource_pool_creation.command_buffers );

    // Create semaphores
    VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for ( size_t i = 0; i < k_max_frames; i++ ) {
        vkCreateSemaphore( vk_device, &semaphore_info, vk_allocation_callbacks, &vk_image_acquired_semaphore[ i ] );
        vkCreateSemaphore( vk_device, &semaphore_info, vk_allocation_callbacks, &vk_render_complete_semaphore[ i ] );
    }

    // Create timeline semaphores to handle graphics and compute work.
    VkSemaphoreTypeCreateInfo semaphore_type_info{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    semaphore_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    semaphore_info.pNext = &semaphore_type_info;

    vkCreateSemaphore( vk_device, &semaphore_info, vk_allocation_callbacks, &vk_graphics_timeline_semaphore );
    vkCreateSemaphore( vk_device, &semaphore_info, vk_allocation_callbacks, &vk_compute_timeline_semaphore );
    vkCreateSemaphore( vk_device, &semaphore_info, vk_allocation_callbacks, &vk_transfer_timeline_semaphore );

    // [TAG: BINDLESS]
    create_bindless_resources();

    // Create common resources
    default_sampler = create_sampler( {
        .min_filter = TextureFilter::Linear, .mag_filter = TextureFilter::Linear,
        .mip_filter = SamplerMipmapMode::Linear, .address_mode_u = SamplerAddressMode::Repeat,
        .address_mode_v = SamplerAddressMode::Repeat, .address_mode_w = SamplerAddressMode::Repeat,
        .debug_name = "default sampler"} );

    dummy_texture = create_texture( {
        .width = 1, .height = 1, .depth = 1, .array_layer_count = 1,
        .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask,
        .format = TextureFormat::R8_UNORM, .type = TextureType::Texture2D,
        .debug_name = "dummy_texture" } );

    staging_buffer = create_buffer( {
        .type = BufferUsage::Staging_mask, .usage = ResourceUsageType::Dynamic,
        .size = imega( 32 ), .persistent = 1, .device_only = 0, .initial_data = nullptr,
        .debug_name = "Staging_buffer" } );

    dynamic_per_frame_size = imega( 1 );
    dynamic_buffer = create_buffer( {
        .type = BufferUsage::Constant_mask ,
        .usage = ResourceUsageType::Dynamic,
        .size = dynamic_per_frame_size * k_max_frames, .persistent = 1, .device_only = 0, .initial_data = nullptr,
        .debug_name = "Dynamic_Persistent_Buffer" } );

    dynamic_mapped_memory = (u8*)map_buffer( dynamic_buffer, 0, 0 );

    // Create swapchain
    create_swapchain();

    shader_compiler_init( creation.shader_folder_path );

    return true;
}

void GpuDevice::internal_shutdown() {

    vkDeviceWaitIdle( vk_device );

    shader_compiler_shutdown();

    unmap_buffer( dynamic_buffer );

    command_buffer_manager->shutdown();
    ifree( command_buffer_manager, allocator );

    // Delete common resources
    destroy_sampler( default_sampler );
    destroy_buffer( staging_buffer );
    destroy_buffer( dynamic_buffer );
    destroy_texture( dummy_texture );
    destroy_swapchain();
    destroy_bindless_resources();

    // Add pending bindless textures to delete.
    for ( u32 i = 0; i < texture_to_update_bindless.size; ++i ) {
        TextureUpdate& update = texture_to_update_bindless[ i ];
        if ( update.deleting ) {
            resource_deletion_queue.push( { {update.texture.index, update.texture.generation}, current_frame, ResourceUpdateType::Texture } );
        }
    }

    delete_queued_resources( true );

    resource_deletion_queue.shutdown();
    texture_uploads.shutdown();
    texture_transfer_completes.shutdown();
    texture_to_update_bindless.shutdown();

    // Free sub-resources slot allocators
    shader_info_allocators[ PipelineType::Graphics ].shutdown();
    shader_info_allocators[ PipelineType::Compute ].shutdown();
    shader_info_allocators[ PipelineType::Raytracing ].shutdown();

    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_2 ].shutdown();
    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_4 ].shutdown();
    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_8 ].shutdown();
    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_16 ].shutdown();
    descriptor_set_bindings_allocators[ DescriptorSetBindingsPools::_32 ].shutdown();

    // Free resource pools
    shader_states.shutdown();
    descriptor_set_layouts.shutdown();
    descriptor_sets.shutdown();
    pipelines.shutdown();
    textures.shutdown();
    samplers.shutdown();
    buffers.shutdown();

    for ( size_t i = 0; i < k_max_frames; i++ ) {
        vkDestroySemaphore( vk_device, vk_render_complete_semaphore[ i ], vk_allocation_callbacks );
        vkDestroySemaphore( vk_device, vk_image_acquired_semaphore[ i ], vk_allocation_callbacks );
    }
    vkDestroySemaphore( vk_device, vk_graphics_timeline_semaphore, vk_allocation_callbacks );
    vkDestroySemaphore( vk_device, vk_compute_timeline_semaphore, vk_allocation_callbacks );
    vkDestroySemaphore( vk_device, vk_transfer_timeline_semaphore, vk_allocation_callbacks );

    vkDestroySurfaceKHR( vk_instance, vk_window_surface, vk_allocation_callbacks );

    vkDestroyDescriptorPool( vk_device, vk_descriptor_pool, vk_allocation_callbacks );

    // Put this here so that pools catch which kind of resource has leaked.
    vmaDestroyAllocator( vma_allocator );

    vkDestroyDevice( vk_device, vk_allocation_callbacks );

#ifdef _DEBUG
    vkDestroyDebugUtilsMessengerEXT( vk_instance, vk_debug_utils_messenger, vk_allocation_callbacks );
#endif

    vkDestroyInstance( vk_instance, vk_allocation_callbacks );

    volkFinalize();
}

// TODO
#define VK_CHECK_SWAPCHAIN(call) \
	do { \
		VkResult result_ = call; \
		assert(result_ == VK_SUCCESS || result_ == VK_SUBOPTIMAL_KHR || result_ == VK_ERROR_OUT_OF_DATE_KHR); \
	} while (0)


void GpuDevice::frame_counters_advance() {
    previous_frame = current_frame;
    current_frame = ( current_frame + 1 ) % swapchain_image_count;

    ++absolute_frame;
}

void GpuDevice::delete_queued_resources( bool force_deletion ) {

    if ( resource_deletion_queue.size > 0 ) {
        for ( i32 i = resource_deletion_queue.size - 1; i >= 0; i-- ) {
            ResourceUpdate& resource_deletion = resource_deletion_queue[ i ];

            // Skip just freed resources.
            if ( resource_deletion.current_frame == u32_max )
                continue;

            if ( resource_deletion.current_frame == current_frame || force_deletion ) {

                switch ( resource_deletion.type ) {

                    case ResourceUpdateType::Buffer:
                    {
                        BufferHandle buffer_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        VulkanBuffer* vk_buffer = buffers.get_hot( buffer_handle );
                        Buffer* buffer = buffers.get_cold( buffer_handle );

                        if ( buffer ) {
                            vmaDestroyBuffer( vma_allocator, vk_buffer->vk_buffer, buffer->vma_allocation );
                        }
                        buffers.destroy_object( buffer_handle );
                        break;
                    }

                    case ResourceUpdateType::Pipeline:
                    {
                        PipelineHandle pipeline_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        VulkanPipeline* v_pipeline = pipelines.get_hot( pipeline_handle );

                        if ( v_pipeline ) {
                            vkDestroyPipeline( vk_device, v_pipeline->vk_pipeline, vk_allocation_callbacks );

                            vkDestroyPipelineLayout( vk_device, v_pipeline->vk_pipeline_layout, vk_allocation_callbacks );
                        }
                        pipelines.destroy_object( pipeline_handle );

                        break;
                    }

                    case ResourceUpdateType::RenderPass:
                    {
                        //destroy_render_pass_instant( resource_deletion.handle );
                        break;
                    }

                    case ResourceUpdateType::Framebuffer:
                    {
                        //destroy_framebuffer_instant( resource_deletion.handle );
                        break;
                    }

                    case ResourceUpdateType::DescriptorSet:
                    {
                        DescriptorSetHandle dst_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        DescriptorSet* v_descriptor_set = descriptor_sets.get_cold( dst_handle );

                        if ( v_descriptor_set ) {
                            // Contains the allocation for all the resources, binding and samplers arrays.
                            //ifree( v_descriptor_set->resources, allocator );
                            // This is freed with the DescriptorSet pool.
                            //vkFreeDescriptorSets
                        }
                        descriptor_sets.destroy_object( dst_handle );
                        break;
                    }

                    case ResourceUpdateType::DescriptorSetLayout:
                    {
                        DescriptorSetLayoutHandle dstl_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        DescriptorSetLayout* v_descriptor_set_layout = descriptor_set_layouts.get_cold( dstl_handle );
                        VulkanDescriptorSetLayout* vk_descriptor_set_layout = descriptor_set_layouts.get_hot( dstl_handle );

                        if ( v_descriptor_set_layout ) {
                            vkDestroyDescriptorSetLayout( vk_device, vk_descriptor_set_layout->vk_descriptor_set_layout, vk_allocation_callbacks );

                            // This contains also vk_binding allocation.
                            DescriptorSetBindingsPools::Enum pool_index = get_binding_allocator_index( v_descriptor_set_layout->num_bindings + v_descriptor_set_layout->num_dynamic_bindings );
                            iassert( pool_index < DescriptorSetBindingsPools::_Count );
                            Allocator* ds_allocator = &descriptor_set_bindings_allocators[ pool_index ];
                            ifree( v_descriptor_set_layout->vk_binding, ds_allocator );
                        }
                        descriptor_set_layouts.destroy_object( dstl_handle );
                        break;
                    }

                    case ResourceUpdateType::Sampler:
                    {
                        SamplerHandle sampler_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        VulkanSampler* v_sampler = samplers.get_hot( sampler_handle );

                        if ( v_sampler ) {
                            vkDestroySampler( vk_device, v_sampler->vk_sampler, vk_allocation_callbacks );
                        }
                        samplers.destroy_object( sampler_handle );
                        break;
                    }

                    case ResourceUpdateType::ShaderState:
                    {
                        ShaderStateHandle shader_state_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        ShaderState* v_shader_state = shader_states.get_cold( shader_state_handle );
                        if ( v_shader_state ) {

                            switch ( v_shader_state->pipeline_type ) {
                                case PipelineType::Compute:
                                {
                                    iassert( v_shader_state->num_active_shaders == 1 );
                                    vkDestroyShaderModule( vk_device, v_shader_state->shader_stage_info[ 0 ].module, vk_allocation_callbacks );
                                    ifree( v_shader_state->shader_stage_info, &shader_info_allocators[ PipelineType::Compute ] );

                                    break;
                                }
                                case PipelineType::Graphics:
                                {
                                    // Take in consideration vertex shader only shaders!
                                    iassert( v_shader_state->num_active_shaders <= 2 );

                                    for ( u32 i = 0; i < v_shader_state->num_active_shaders; ++i ) {
                                        vkDestroyShaderModule( vk_device, v_shader_state->shader_stage_info[ i ].module, vk_allocation_callbacks );
                                    }
                                    ifree( v_shader_state->shader_stage_info, &shader_info_allocators[ PipelineType::Graphics ] );
                                    break;
                                }
                                case PipelineType::Raytracing:
                                {
                                    iassert( false );
                                    break;
                                }
                                default:
                                {
                                    iassert( false );
                                    break;
                                }

                            }
                        }
                        shader_states.destroy_object( shader_state_handle );
                        break;
                    }

                    case ResourceUpdateType::Texture:
                    {
                        TextureHandle texture_handle{ resource_deletion.handle.index, resource_deletion.handle.generation };
                        VulkanTexture* vk_texture = textures.get_hot( texture_handle );

                        // Skip double frees.
                        if ( !vk_texture->vk_image_view ) {
                            return;
                        }

                        Texture* v_texture = textures.get_cold( texture_handle );

                        if ( v_texture ) {
                            // Default texture view added as separate destroy command.
                            vkDestroyImageView( vk_device, vk_texture->vk_image_view, vk_allocation_callbacks );
                            vk_texture->vk_image_view = VK_NULL_HANDLE;

                            // Standard texture: vma allocation valid, and is NOT a texture view (parent_texture is invalid)
                            if ( v_texture->vma_allocation != 0 && v_texture->parent_texture.is_invalid() ) {
                                vmaDestroyImage( vma_allocator, vk_texture->vk_image, v_texture->vma_allocation );
                            } else if ( ( v_texture->flags & TextureFlags::Sparse_mask ) == TextureFlags::Sparse_mask ) {
                                // Sparse textures
                                vkDestroyImage( vk_device, vk_texture->vk_image, vk_allocation_callbacks );
                            } else if ( v_texture->vma_allocation == nullptr ) {
                                // Aliased textures
                                vkDestroyImage( vk_device, vk_texture->vk_image, vk_allocation_callbacks );
                            }
                        }
                        textures.destroy_object( texture_handle );
                        break;
                    }
                }

                // Mark resource as free
                resource_deletion.current_frame = u32_max;

                // Swap element
                resource_deletion_queue.delete_swap( i );
            }
        }
    }
}

void GpuDevice::new_frame() {

    /*static VmaBudget gpu_heap_budgets[ 16 ];
    const u32 gpu_heaps = vma_allocator->GetMemoryHeapCount();
    vmaGetHeapBudgets( vma_allocator, gpu_heap_budgets );

    sizet memory_used = 0;
    sizet memory_allocated = 0;
    for ( u32 i = 0; i < vma_allocator->GetMemoryHeapCount(); ++i ) {
        memory_used += gpu_heap_budgets[ i ].usage;
        memory_allocated += gpu_heap_budgets[ i ].budget;
    }

    ilog( "GPU Memory Used: %lluMB, Total: %lluMB\n", memory_used / ( 1024 * 1024 ), memory_allocated / ( 1024 * 1024 ) );*/

    iassertm( k_max_frames <= swapchain_image_count, "Cannot have more frame in flights than swapchains!" );

    // TODO: try to use the actual swapchain count.
    //if ( absolute_frame >= k_max_frames ) {
    if ( absolute_frame >= swapchain_image_count ) {
        //u64 graphics_timeline_value = absolute_frame - ( k_max_frames - 1 );
        const u64 graphics_timeline_value = absolute_frame - ( swapchain_image_count - 1 );
        const u64 compute_timeline_value = last_compute_semaphore_value;
        const u64 transfer_timeline_value = last_transfer_semaphore_value;

        u64 wait_values[ 3 ] = { graphics_timeline_value, 0, 0 };
        VkSemaphore semaphores[ 3 ] = { vk_graphics_timeline_semaphore, 0, 0 };

        u32 num_semaphores = 1;

        if ( has_transfer_work ) {
            wait_values[ num_semaphores ] = transfer_timeline_value;
            semaphores[ num_semaphores ] = vk_transfer_timeline_semaphore;
            ++num_semaphores;
        }

        if ( has_async_work ) {
            wait_values[ num_semaphores ] = compute_timeline_value;
            semaphores[ num_semaphores ] = vk_compute_timeline_semaphore;
            ++num_semaphores;
        }

        VkSemaphoreWaitInfo semaphore_wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
        semaphore_wait_info.semaphoreCount = num_semaphores;
        semaphore_wait_info.pSemaphores = semaphores;
        semaphore_wait_info.pValues = wait_values;

        vkWaitSemaphores( vk_device, &semaphore_wait_info, ~0ull );
    }

    VK_CHECK_SWAPCHAIN( vkAcquireNextImageKHR( vk_device, vk_swapchain, u64_max, vk_image_acquired_semaphore[ current_frame ], VK_NULL_HANDLE, &swapchain_image_index));

    // Move allocated size to free part of the buffer.
    //const u32 used_size = dynamic_allocated_size - ( dynamic_per_frame_size * previous_frame );
    //dynamic_max_per_frame_size = idra_max( used_size, dynamic_max_per_frame_size );
    dynamic_allocated_size = dynamic_per_frame_size * current_frame;

    // Free all command buffers
    command_buffer_manager->free_unused_buffers( current_frame );

    // TODO: add also buffer uploads
    has_transfer_work = texture_uploads.size > 0;

    CommandBuffer* cb = command_buffer_manager->get_transfer_command_buffer();

    // Execute transfer operations
    if ( texture_uploads.size ) {

        // Go through all upload requests
        for ( u32 i = 0; i < texture_uploads.size; ++i ) {
            UploadTextureData& upload = texture_uploads[ i ];

            Texture* texture = textures.get_cold( upload.texture );
            u32 image_size = (u32)GpuUtils::calculate_texture_size( texture );

            cb->upload_texture_data( upload.texture, upload.data, staging_buffer, staging_buffer_offset );

            staging_buffer_offset += image_size;

            // Add to texture to finish transfer
            texture_transfer_completes.push( upload );
        }

        // Reset staging buffer
        staging_buffer_offset = 0;
        // Reset texture upload requests
        texture_uploads.clear();

        vkEndCommandBuffer( cb->vk_command_buffer );

        submit_transfer_work( cb );
    }
}

void GpuDevice::submit_transfer_work( CommandBuffer* command_buffer ) {
    VkCommandBufferSubmitInfoKHR command_buffer_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR, .pNext = nullptr,
        .commandBuffer = command_buffer->vk_command_buffer };

    VkSemaphoreSubmitInfoKHR wait_semaphores{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
        .semaphore = vk_transfer_timeline_semaphore, .value = last_transfer_semaphore_value,
        .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .deviceIndex = 0
    };

    VkSemaphoreSubmitInfoKHR signal_semaphores{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
        .semaphore = vk_transfer_timeline_semaphore, .value = last_transfer_semaphore_value + 1,
        .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT , .deviceIndex = 0
    };

    VkSubmitInfo2KHR submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR, .pNext = nullptr, .flags = 0,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_semaphores,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &command_buffer_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_semaphores
    };

    vkcheck( vkQueueSubmit2KHR( vk_queues[ QueueType::Transfer ], 1, &submit_info, 0 ) );

    ++last_transfer_semaphore_value;
    has_transfer_work = true;
}

TextureHandle GpuDevice::get_current_swapchain_texture() {
    return swapchain_textures[ swapchain_image_index ];
}

void GpuDevice::enqueue_command_buffer( CommandBuffer* command_buffer ) {
    enqueued_command_buffers[ num_enqueued_command_buffers++ ] = command_buffer;
}

void GpuDevice::present() {

    // TODO: improve with a fence ?

    if ( texture_transfer_completes.size ) {

        CommandBuffer* cb = acquire_command_buffer( 0 );

        for ( u32 i = 0; i < texture_transfer_completes.size; ++i ) {
            UploadTextureData& upload = texture_transfer_completes[ i ];

            // Add final barrier
            //cb->add_texture_barrier( upload.texture, ResourceState::ShaderResource, 0, 1, QueueType::Transfer, QueueType::Graphics );

            VkImageMemoryBarrier2* barrier = &cb->vk_image_barriers[ cb->num_vk_image_barriers++ ];
            Texture* texture = textures.get_cold( upload.texture );
            VulkanTexture* vk_texture = textures.get_hot( upload.texture );
            bool is_depth = TextureFormat::has_depth( texture->format );

            // TODO: texture should be in CopySources, but it is in CopyDest.
            // Manually filling this with old state as CopyDest so validation
            // layer does not complain.
            util_fill_image_barrier( barrier, vk_texture->vk_image, ResourceState::CopyDest, ResourceState::ShaderResource, 0,
                                     1, 0, 1, is_depth,
                                     queue_indices[ QueueType::Transfer ],
                                     queue_indices[ QueueType::Graphics ],
                                     QueueType::Transfer, QueueType::Graphics);

            vk_texture->state = ResourceState::ShaderResource;
        }

        // Submit all barriers
        VkDependencyInfoKHR dependency_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
        dependency_info.imageMemoryBarrierCount = cb->num_vk_image_barriers;
        dependency_info.pImageMemoryBarriers = cb->vk_image_barriers;
        dependency_info.bufferMemoryBarrierCount = 0;
        dependency_info.pBufferMemoryBarriers = nullptr;

        vkCmdPipelineBarrier2KHR( cb->vk_command_buffer, &dependency_info );

        // Restore barrier count to 0
        cb->num_vk_image_barriers = 0;
        cb->num_vk_buffer_barriers = 0;

        texture_transfer_completes.clear();
    }

    if ( texture_to_update_bindless.size ) {
        // Handle deferred writes to bindless textures.
        VkWriteDescriptorSet bindless_descriptor_writes[ k_max_bindless_resources ];
        VkDescriptorImageInfo bindless_image_info[ k_max_bindless_resources ];

        VulkanTexture* vk_dummy_texture = textures.get_hot( dummy_texture );
        VulkanDescriptorSet* vk_descriptor_set = descriptor_sets.get_hot( bindless_descriptor_set );

        u32 current_write_index = 0;
        for ( i32 it = texture_to_update_bindless.size - 1; it >= 0; it-- ) {
            TextureUpdate& texture_to_update = texture_to_update_bindless[ it ];

            //if ( texture_to_update.current_frame == current_frame )
            {
                VulkanTexture* vk_texture = textures.get_hot( texture_to_update.texture );

                if ( vk_texture->vk_image_view == VK_NULL_HANDLE ) {
                    continue;
                }
                
                Texture* texture = textures.get_cold( texture_to_update.texture );

                VkWriteDescriptorSet& descriptor_write = bindless_descriptor_writes[ current_write_index ];
                descriptor_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                descriptor_write.descriptorCount = 1;
                descriptor_write.dstArrayElement = texture_to_update.texture.index;
                descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptor_write.dstSet = vk_descriptor_set->vk_descriptor_set;
                descriptor_write.dstBinding = k_bindless_texture_binding;

                // Handles should be the same.
                iassert( texture->handle == texture_to_update.texture );

                VulkanSampler* vk_default_sampler = samplers.get_hot( default_sampler );
                VkDescriptorImageInfo& descriptor_image_info = bindless_image_info[ current_write_index ];

                // Update image view and sampler if valid
                if ( !texture_to_update.deleting ) {
                    descriptor_image_info.imageView = vk_texture->vk_image_view;

                    if ( texture->sampler.is_valid() ) {
                        VulkanSampler* sampler = samplers.get_hot( texture->sampler );
                        descriptor_image_info.sampler = sampler->vk_sampler;
                    } else {
                        descriptor_image_info.sampler = vk_default_sampler->vk_sampler;
                    }
                } else {
                    // Deleting: set to default image view and sampler in the current slot.
                    descriptor_image_info.imageView = vk_dummy_texture->vk_image_view;
                    descriptor_image_info.sampler = vk_default_sampler->vk_sampler;
                }

                descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                descriptor_write.pImageInfo = &descriptor_image_info;

                texture_to_update.current_frame = u32_max;
                // Cache this value, as delete_swap will modify the texture_to_update reference.
                const bool add_texture_to_delete = texture_to_update.deleting;
                texture_to_update_bindless.delete_swap( it );

                ++current_write_index;

                // Debug
                //if ( strcmp("", texture->name) == 0 ) {
                    //ilog( "%s texture %u\n", add_texture_to_delete ? "Deleting" : "Updating", texture->handle.index );
                //}

                // Add texture to delete
                if ( add_texture_to_delete ) {
                    resource_deletion_queue.push( { {texture->handle.index, texture->handle.generation}, current_frame, ResourceUpdateType::Texture } );
                }

                // Add optional compute bindless descriptor update
                if ( texture->flags & TextureFlags::Compute_mask ) {
                    VkWriteDescriptorSet& descriptor_write_image = bindless_descriptor_writes[ current_write_index ];
                    VkDescriptorImageInfo& descriptor_image_info_compute = bindless_image_info[ current_write_index ];

                    // Copy common data from descriptor and image info
                    descriptor_write_image = descriptor_write;
                    descriptor_image_info_compute = descriptor_image_info;

                    descriptor_image_info_compute.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                    descriptor_write_image.dstBinding = k_bindless_image_binding;
                    descriptor_write_image.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    descriptor_write_image.pImageInfo = &descriptor_image_info_compute;

                    ++current_write_index;
                }
            }
        }

        if ( current_write_index ) {
            vkUpdateDescriptorSets( vk_device, current_write_index, bindless_descriptor_writes, 0, nullptr );
        }
    }

    VkSemaphore* render_complete_semaphore = &vk_render_complete_semaphore[ current_frame ];
    bool wait_for_compute_work = ( last_compute_semaphore_value > 0 ) && has_async_work;
    bool wait_for_transfer_work = ( last_transfer_semaphore_value > 0 ) && has_transfer_work;

    // TODO: try to use the actual swapchain count.
    //bool wait_for_timeline_semaphore = absolute_frame >= k_max_frames;
    bool wait_for_graphics_work = absolute_frame >= swapchain_image_count;
    VkCommandBufferSubmitInfoKHR command_buffer_info[ k_max_enqueued_command_buffers ]{ };

    for ( u32 c = 0; c < num_enqueued_command_buffers; c++ ) {
        command_buffer_info[ c ].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
        command_buffer_info[ c ].commandBuffer = enqueued_command_buffers[ c ]->vk_command_buffer;

        // End command buffer
        vkEndCommandBuffer( enqueued_command_buffers[ c ]->vk_command_buffer );
    }

    Array<VkSemaphoreSubmitInfoKHR> wait_semaphores;
    wait_semaphores.init( g_memory->get_thread_allocator(), 4 );

    wait_semaphores.push( {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
        .semaphore = vk_image_acquired_semaphore[ current_frame ], .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, .deviceIndex = 0 } );

    if ( wait_for_compute_work ) {
        wait_semaphores.push( {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
            .semaphore = vk_compute_timeline_semaphore, .value = last_compute_semaphore_value,
            .stageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR, .deviceIndex = 0 } );
    }

    if ( wait_for_transfer_work ) {
        wait_semaphores.push( {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
            .semaphore = vk_transfer_timeline_semaphore, .value = last_transfer_semaphore_value,
            .stageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR, .deviceIndex = 0 } );
    }

    if ( wait_for_graphics_work ) {
        // TODO: try to use the actual swapchain count.
        //wait_semaphores.push( { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr, vk_graphics_semaphore, absolute_frame - ( k_max_frames - 1 ), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR , 0 } );
        wait_semaphores.push( {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
            .semaphore = vk_graphics_timeline_semaphore, .value = absolute_frame - ( swapchain_image_count - 1 ),
            .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR , .deviceIndex = 0 } );
    }

    // Render complete semaphore is just signalled or not, while the timeline
    // semaphore updates its value when done.
    VkSemaphoreSubmitInfoKHR signal_semaphores[] {
        { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
          .semaphore = *render_complete_semaphore, .value = 0,
          .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, .deviceIndex = 0 },

        { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, .pNext = nullptr,
          .semaphore = vk_graphics_timeline_semaphore, .value = absolute_frame + 1,
          .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR , .deviceIndex = 0 }
    };

    VkSubmitInfo2KHR submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR, .pNext = nullptr, .flags = 0,
        .waitSemaphoreInfoCount = wait_semaphores.size,
        .pWaitSemaphoreInfos = wait_semaphores.data,
        .commandBufferInfoCount = num_enqueued_command_buffers,
        .pCommandBufferInfos = command_buffer_info,
        .signalSemaphoreInfoCount = 2,
        .pSignalSemaphoreInfos = signal_semaphores
    };

    vkcheck( vkQueueSubmit2KHR( vk_queues[ QueueType::Graphics ], 1, &submit_info, VK_NULL_HANDLE));

    // Reset enqueued command buffers count
    num_enqueued_command_buffers = 0;
    has_transfer_work = false;
    has_async_work = false;

    wait_semaphores.shutdown();

    // TODO: async compute
    /*if ( async_compute_command_buffer != nullptr ) {
        submit_compute_load( async_compute_command_buffer );
    }*/

    VkPresentInfoKHR present_info {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .pNext = nullptr,
        .waitSemaphoreCount = 1, .pWaitSemaphores = render_complete_semaphore,
        .swapchainCount = 1, .pSwapchains = &vk_swapchain,
        .pImageIndices = &swapchain_image_index, .pResults = nullptr };

    VkResult result = vkQueuePresentKHR( vk_queues[ QueueType::Graphics ], &present_info );
    iassert( result != VK_ERROR_DEVICE_LOST );

    // Time queries ///////////////////////////////////////////////////////
    //
    // GPU Timestamp resolve
    if ( true /*timestamps_enabled*/ ) {

        // Reset the frame statistics
        //gpu_time_queries_manager->frame_pipeline_statistics.reset();
        BookmarkAllocator* temporary_allocator = g_memory->get_thread_allocator();
        temporary_allocator->clear();

        Span<CommandBuffer> span = command_buffer_manager->get_command_buffer_span( previous_frame );

        for ( u32 q = 0; q < span.size; ++q ) {

            CommandBuffer* command_buffer = &span[ q ];
            GpuTimeQueryTree* time_query = &command_buffer->time_query_tree;

            // For each active time query pool
            if ( time_query->allocated_time_query ) {

                // Query GPU for all timestamps.
                const u32 query_count = time_query->allocated_time_query;
                u64* timestamps_data = ( u64* )ialloc( query_count * 2 * sizeof( u64 ), temporary_allocator );
                vkGetQueryPoolResults( vk_device, command_buffer->vk_time_query_pool,
                                       0, query_count * 2,
                                       sizeof( u64 ) * query_count * 2, timestamps_data,
                                       sizeof( u64 ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );

                // Calculate and cache the elapsed time
                for ( u32 i = 0; i < query_count; i++ ) {

                    GPUTimeQuery& timestamp = command_buffer->time_query_tree.time_queries[ i ];

                    double start = ( double )timestamps_data[ ( i * 2 ) ];
                    double end = ( double )timestamps_data[ ( i * 2 ) + 1 ];
                    double range = end - start;
                    double elapsed_time = range * gpu_timestamp_frequency;

                    timestamp.elapsed_ms = elapsed_time;
                    timestamp.frame_index = absolute_frame;

                    //ilog_debug( "%s: %2.3f d(%u) - \n", timestamp.name.data, elapsed_time, timestamp.depth );
                }

                //// Query and sum pipeline statistics
                //u64* pipeline_statistics_data = ( u64* )ralloca( GpuPipelineStatistics::Count * sizeof( u64 ), temporary_allocator );
                //vkGetQueryPoolResults( vulkan_device, thread_pool.vulkan_pipeline_stats_query_pool, 0, 1,
                //                       GpuPipelineStatistics::Count * sizeof( u64 ), pipeline_statistics_data, sizeof( u64 ), VK_QUERY_RESULT_64_BIT );

                //for ( u32 i = 0; i < GpuPipelineStatistics::Count; ++i ) {
                //    gpu_time_queries_manager->frame_pipeline_statistics.statistics[ i ] += pipeline_statistics_data[ i ];
                //}
            }

            temporary_allocator->clear();
        }

        // Query results from previous frame.
        for ( u32 i = 0; i < 1; ++i ) {


        }

        //ilog( "%llu %f\n", gpu_time_queries_manager->frame_pipeline_statistics.statistics[ 6 ], ( gpu_time_queries_manager->frame_pipeline_statistics.statistics[ 6 ] * 1.0 ) / ( swapchain_width * swapchain_height ) );
    }

    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR /*|| resized*/ ) {
        //resized = false;
        //resize_swapchain();

        // Advance frame counters that are skipped during this frame.
        frame_counters_advance();

        return;
    }

    frame_counters_advance();

    delete_queued_resources( false );
}

void GpuDevice::create_bindless_resources() {

    if ( !bindless_supported ) {
        ilog_debug( "Bindless not supported - no bindless resources will be created.\n" );
        return;
    }

    // Create the Descriptor Pool used by bindless, that needs update after bind flag.
    VkDescriptorPoolSize pool_sizes_bindless[] =
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_max_bindless_resources },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, k_max_bindless_resources },
    };

    VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    // Update after bind is needed here, for each binding and in the descriptor set layout creation.
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    pool_info.maxSets = k_max_bindless_resources * ArraySize( pool_sizes_bindless );
    pool_info.poolSizeCount = ( u32 )ArraySize( pool_sizes_bindless );
    pool_info.pPoolSizes = pool_sizes_bindless;
    vkcheck( vkCreateDescriptorPool( vk_device, &pool_info, vk_allocation_callbacks, &vk_bindless_descriptor_pool ) );

    // Create bindless descriptor set
    bindless_descriptor_set_layout = create_bindless_descriptor_set_layout( {
        .bindings = {
            { .type = DescriptorType::Texture, .start = k_bindless_texture_binding, .count = k_max_bindless_resources, .name = "src" },
            { .type = DescriptorType::Image, .start = k_bindless_image_binding, .count = k_max_bindless_resources, .name = "dst" },
        },
        .debug_name = "bindless_dsl" } );

    // Create bindless descriptor set layout
    bindless_descriptor_set = create_descriptor_set( {
        .layout = bindless_descriptor_set_layout,
        .debug_name = "bindless_ds" } );
}

void GpuDevice::destroy_bindless_resources() {
    if ( !bindless_supported ) {
        ilog_debug( "Bindless not supported - no bindless resources will be destroyed.\n" );
        return;
    }

    destroy_descriptor_set_layout( bindless_descriptor_set_layout );
    destroy_descriptor_set( bindless_descriptor_set );

    vkDestroyDescriptorPool( vk_device, vk_bindless_descriptor_pool, vk_allocation_callbacks );
}

// Resource management
BufferHandle GpuDevice::create_buffer( const BufferCreation& creation ) {

    BufferHandle handle = buffers.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::Buffer, handle.index, creation.name );

    Buffer* buffer = buffers.get_cold( handle );
    VulkanBuffer* vk_buffer = buffers.get_hot( handle );

    buffer->name = creation.debug_name;
    buffer->size = creation.size;
    //buffer->type_flags = creation.type_flags;
    buffer->usage = creation.usage;
    buffer->handle = handle;
    buffer->state = ResourceState::Undefined;

    VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if ( ( creation.type & BufferUsage::Constant_mask ) == BufferUsage::Constant_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if ( ( creation.type & BufferUsage::Structured_mask ) == BufferUsage::Structured_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if ( ( creation.type & BufferUsage::Indirect_mask ) == BufferUsage::Indirect_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    if ( ( creation.type & BufferUsage::Vertex_mask ) == BufferUsage::Vertex_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if ( ( creation.type & BufferUsage::Index_mask ) == BufferUsage::Index_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if ( ( creation.type & BufferUsage::Staging_mask ) == BufferUsage::Staging_mask ) {
        buffer_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | buffer_usage;
    buffer_info.size = creation.size > 0 ? creation.size : 1;       // 0 sized creations are not permitted.

    // NOTE(marco): technically we could map a buffer if the device exposes a heap
    // with MEMORY_PROPERTY_DEVICE_LOCAL_BIT and MEMORY_PROPERTY_HOST_VISIBLE_BIT
    // but that's usually very small (256MB) unless resizable bar is enabled.
    // We simply don't allow it for now.
    iassert( !( creation.persistent && creation.device_only ) );

    VmaAllocationCreateInfo allocation_create_info{};
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    if ( creation.persistent ) {
        allocation_create_info.flags = allocation_create_info.flags | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    if ( creation.device_only ) {
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    } else {
        allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    }

    VmaAllocationInfo allocation_info{};
    vkcheck( vmaCreateBuffer( vma_allocator, &buffer_info, &allocation_create_info,
                            &vk_buffer->vk_buffer, &buffer->vma_allocation, &allocation_info ) );
#if defined (_DEBUG)
    vmaSetAllocationName( vma_allocator, buffer->vma_allocation, creation.debug_name.data );
#endif // _DEBUG

    set_resource_name( VK_OBJECT_TYPE_BUFFER, ( u64 )vk_buffer->vk_buffer, creation.debug_name.data );

    buffer->vk_device_memory = allocation_info.deviceMemory;

    if ( creation.initial_data ) {
        void* data;
        vmaMapMemory( vma_allocator, buffer->vma_allocation, &data );
        memcpy( data, creation.initial_data, ( size_t )creation.size );
        vmaUnmapMemory( vma_allocator, buffer->vma_allocation );
    }

    if ( creation.persistent ) {
        buffer->mapped_data = static_cast< u8* >( allocation_info.pMappedData );
    }

    return handle;
}
static void vulkan_create_texture_view( GpuDevice& gpu, const TextureViewCreation& creation, Texture* texture, VulkanTexture* vk_texture ) {

    // Create the image view
    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    info.image = vk_texture->vk_image;
    info.format = ( VkFormat )texture->vk_format;

    if ( TextureFormat::has_depth_or_stencil( texture->format ) )
    {
        info.subresourceRange.aspectMask = TextureFormat::has_depth( texture->format ) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
        // TODO:gs
        //info.subresourceRange.aspectMask |= TextureFormat::has_stencil( creation.format ) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
    } else {
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    info.viewType = to_vk_image_view_type( creation.view_type );
    info.subresourceRange.baseMipLevel = creation.sub_resource.mip_base_level;
    info.subresourceRange.levelCount = creation.sub_resource.mip_level_count;
    info.subresourceRange.baseArrayLayer = creation.sub_resource.array_base_layer;
    info.subresourceRange.layerCount = creation.sub_resource.array_layer_count;
    vkcheck( vkCreateImageView( gpu.vk_device, &info, gpu.vk_allocation_callbacks, &vk_texture->vk_image_view ) );

    gpu.set_resource_name( VK_OBJECT_TYPE_IMAGE_VIEW, ( u64 )vk_texture->vk_image_view, creation.debug_name );
}

static VkImageUsageFlags vulkan_get_image_usage( const TextureCreation& creation ) {
    const bool is_render_target = ( creation.flags & TextureFlags::RenderTarget_mask ) == TextureFlags::RenderTarget_mask;
    const bool is_compute_used = ( creation.flags & TextureFlags::Compute_mask ) == TextureFlags::Compute_mask;
    const bool is_shading_rate_texture = ( creation.flags & TextureFlags::ShadingRate_mask ) == TextureFlags::ShadingRate_mask;

    // Default to always readable from shader.
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    usage |= is_compute_used ? VK_IMAGE_USAGE_STORAGE_BIT : 0;

    usage |= is_shading_rate_texture ? VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR : 0;

    if ( TextureFormat::has_depth_or_stencil( creation.format ) ) {
        // Depth/Stencil textures are normally textures you render into.
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // TODO

    } else {
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // TODO
        usage |= is_render_target ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    }

    return usage;
}

static void vulkan_create_texture( GpuDevice& gpu, const TextureCreation& creation, TextureHandle handle, Texture* texture, VulkanTexture* vk_texture ) {

    bool is_cubemap = false;
    u32 layer_count = creation.array_layer_count;
    if ( creation.type == TextureType::TextureCube || creation.type == TextureType::Texture_Cube_Array ) {
        is_cubemap = true;
    }

    const bool is_sparse_texture = ( creation.flags & TextureFlags::Sparse_mask ) == TextureFlags::Sparse_mask;

    texture->width = creation.width;
    texture->height = creation.height;
    texture->depth = creation.depth;
    texture->mip_base_level = 0;        // For new textures, we have a view that is for all mips and layers.
    texture->array_base_layer = 0;      // For new textures, we have a view that is for all mips and layers.
    texture->array_layer_count = layer_count;
    texture->mip_level_count = creation.mip_level_count;
    texture->type = creation.type;
    texture->name = creation.debug_name;
    texture->vk_format = to_vk_format( creation.format );
    texture->format = creation.format;
    texture->vk_usage = vulkan_get_image_usage( creation );
    texture->flags = creation.flags;
    texture->parent_texture = { 0,0 };
    texture->handle = handle;
    texture->sparse = is_sparse_texture;
    texture->alias_texture = { 0,0 };
    texture->sampler = creation.sampler;

    //// Create the image
    VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.format = ( VkFormat )texture->vk_format;
    image_info.flags = ( is_cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0 ) | ( is_sparse_texture ? ( VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT ) : 0 );
    image_info.imageType = to_vk_image_type( texture->type );
    image_info.extent.width = creation.width;
    image_info.extent.height = creation.height;
    image_info.extent.depth = creation.depth;
    image_info.mipLevels = creation.mip_level_count;
    image_info.arrayLayers = layer_count;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = texture->vk_usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo memory_info{};
    memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ilog_debug( "creating tex %s\n", creation.debug_name.data );

    if ( creation.alias.is_invalid() ) {
        if ( is_sparse_texture ) {
            vkcheck( vkCreateImage( gpu.vk_device, &image_info, gpu.vk_allocation_callbacks, &vk_texture->vk_image ) );
        } else {
            vkcheck( vmaCreateImage( gpu.vma_allocator, &image_info, &memory_info,
                                   &vk_texture->vk_image, &texture->vma_allocation, nullptr ) );

#if defined (_DEBUG)
            vmaSetAllocationName( gpu.vma_allocator, texture->vma_allocation, creation.debug_name.data );
#endif // _DEBUG
        }
    } else {
        Texture* alias_texture = gpu.textures.get_cold( creation.alias );
        iassert( alias_texture != nullptr );
        iassert( !is_sparse_texture );

        texture->vma_allocation = 0;
        vkcheck( vmaCreateAliasingImage( gpu.vma_allocator, alias_texture->vma_allocation, &image_info, &vk_texture->vk_image ) );
        texture->alias_texture = creation.alias;
    }

    gpu.set_resource_name( VK_OBJECT_TYPE_IMAGE, ( u64 )vk_texture->vk_image, creation.debug_name );

    // Create default texture view.
    TextureViewCreation tvc = {
        .parent_texture = {0,0}, .view_type = creation.type,
        .sub_resource = {
            .mip_base_level = 0, .mip_level_count = creation.mip_level_count,
            .array_base_layer = 0, .array_layer_count = (u16)layer_count
        },
        .debug_name = creation.debug_name
    };

    vulkan_create_texture_view( gpu, tvc, texture, vk_texture );
    vk_texture->state = ResourceState::Undefined;

    // Add deferred bindless update.
    if ( gpu.bindless_supported ) {
        gpu.texture_to_update_bindless.push( { texture->handle, gpu.current_frame, 0 } );
    }
}

//static void upload_texture_data( Texture* texture, void* upload_data, GpuDevice& gpu ) {

    //// Create stating buffer
    //VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    //buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    //u32 image_size = texture->width * texture->height * 4;
    //buffer_info.size = image_size;

    //VmaAllocationCreateInfo memory_info{};
    //memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    //memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    //VmaAllocationInfo allocation_info{};
    //VkBuffer staging_buffer;
    //VmaAllocation staging_allocation;
    //( vmaCreateBuffer( gpu.vma_allocator, &buffer_info, &memory_info,
    //                   &staging_buffer, &staging_allocation, &allocation_info ) );

    //// Copy buffer_data
    //void* destination_data;
    //vmaMapMemory( gpu.vma_allocator, staging_allocation, &destination_data );
    //memcpy( destination_data, upload_data, static_cast< size_t >( image_size ) );
    //vmaUnmapMemory( gpu.vma_allocator, staging_allocation );

    //// Execute command buffer
    //VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    //// TODO: threading
    //CommandBuffer* command_buffer = gpu.get_command_buffer( 0, gpu.current_frame, false );
    //vkBeginCommandBuffer( command_buffer->vk_command_buffer, &beginInfo );

    //VkBufferImageCopy region = {};
    //region.bufferOffset = 0;
    //region.bufferRowLength = 0;
    //region.bufferImageHeight = 0;

    //region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //region.imageSubresource.mipLevel = 0;
    //region.imageSubresource.baseArrayLayer = 0;
    //region.imageSubresource.layerCount = 1;

    //region.imageOffset = { 0, 0, 0 };
    //region.imageExtent = { texture->width, texture->height, texture->depth };

    //// Copy from the staging buffer to the image
    //util_add_image_barrier( &gpu, command_buffer->vk_command_buffer, texture->vk_image, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST, 0, 1, false );

    //vkCmdCopyBufferToImage( command_buffer->vk_command_buffer, staging_buffer, texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
    //// Prepare first mip to create lower mipmaps
    //if ( texture->mip_level_count > 1 ) {
    //    util_add_image_barrier( &gpu, command_buffer->vk_command_buffer, texture->vk_image, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_SOURCE, 0, 1, false );
    //}

    //i32 w = texture->width;
    //i32 h = texture->height;

    //for ( int mip_index = 1; mip_index < texture->mip_level_count; ++mip_index ) {
    //    util_add_image_barrier( &gpu, command_buffer->vk_command_buffer, texture->vk_image, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST, mip_index, 1, false );

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

    //    vkCmdBlitImage( command_buffer->vk_command_buffer, texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_LINEAR );

    //    // Prepare current mip for next level
    //    util_add_image_barrier( &gpu, command_buffer->vk_command_buffer, texture->vk_image, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COPY_SOURCE, mip_index, 1, false );
    //}

    //// Transition
    //util_add_image_barrier( &gpu, command_buffer->vk_command_buffer, texture->vk_image, ( texture->mip_level_count > 1 ) ? RESOURCE_STATE_COPY_SOURCE : RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE, 0, texture->mip_level_count, false );
    //texture->state = RESOURCE_STATE_SHADER_RESOURCE;

    //vkEndCommandBuffer( command_buffer->vk_command_buffer );

    //// Submit command buffer
    //if ( gpu.synchronization2_extension_present ) {
    //    VkCommandBufferSubmitInfoKHR command_buffer_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR };
    //    command_buffer_info.commandBuffer = command_buffer->vk_command_buffer;

    //    VkSubmitInfo2KHR submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR };
    //    submit_info.commandBufferInfoCount = 1;
    //    submit_info.pCommandBufferInfos = &command_buffer_info;

    //    gpu.vkQueueSubmit2KHR( gpu.vulkan_main_queue, 1, &submit_info, VK_NULL_HANDLE );
    //} else {
    //    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    //    submitInfo.commandBufferCount = 1;
    //    submitInfo.pCommandBuffers = &command_buffer->vk_command_buffer;

    //    vkQueueSubmit( gpu.vulkan_main_queue, 1, &submitInfo, VK_NULL_HANDLE );
    //}
    //vkQueueWaitIdle( gpu.vulkan_main_queue );

    //vmaDestroyBuffer( gpu.vma_allocator, staging_buffer, staging_allocation );

    //// TODO: free command buffer
    //vkResetCommandBuffer( command_buffer->vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT );
//}

TextureHandle GpuDevice::create_texture( const TextureCreation& creation ) {

    TextureHandle handle = textures.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::Texture, resource_index, creation.name );

    Texture* texture = textures.get_cold( handle );
    VulkanTexture* vk_texture = textures.get_hot( handle );

    vulkan_create_texture( *this, creation, handle, texture, vk_texture );

    //// Copy buffer_data if present
    if ( creation.initial_data ) {
        upload_texture_data( handle, creation.initial_data );
    }
    return handle;
}

TextureHandle GpuDevice::create_texture_view( const TextureViewCreation& creation ) {

    TextureHandle handle = textures.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::Texture, resource_index, creation.name );
    Texture* parent_texture = textures.get_cold( creation.parent_texture );
    VulkanTexture* vk_parent_texture = textures.get_hot( creation.parent_texture );

    Texture* texture_view = textures.get_cold( handle );
    VulkanTexture* vk_texture_view = textures.get_hot( handle );

    // Copy parent texture data to texture view
    memcpy( texture_view, parent_texture, sizeof( Texture ) );
    // Copy parent VulkanTexture data to texture view as well
    memcpy( vk_texture_view, vk_parent_texture, sizeof( VulkanTexture ) );

    // Add texture view data
    texture_view->parent_texture = creation.parent_texture;
    texture_view->handle = handle;
    texture_view->array_base_layer = creation.sub_resource.array_base_layer;
    texture_view->mip_base_level = creation.sub_resource.mip_base_level;

    vulkan_create_texture_view( *this, creation, texture_view, vk_texture_view );

    return handle;
}

static bool create_shader_module( GpuDevice& gpu, const ShaderStageCode& shader, VkPipelineShaderStageCreateInfo& out_shader_stage );

ShaderStateHandle GpuDevice::create_graphics_shader_state( const GraphicsShaderStateCreation& creation ) {

    ShaderStateHandle handle = shader_states.obtain_object();

    if ( handle.is_invalid() ) {
        return handle;
    }

    ShaderState* shader_state = shader_states.get_cold( handle );
    VulkanShaderState* vk_shader_state = shader_states.get_hot( handle );

    shader_state->pipeline_type = PipelineType::Graphics;
    shader_state->num_active_shaders = 0;
    shader_state->shader_group_info = nullptr;
    shader_state->shader_stage_info = ( VkPipelineShaderStageCreateInfo* )ialloc( sizeof( VkPipelineShaderStageCreateInfo ) * 2, &shader_info_allocators[ PipelineType::Graphics ] );

    if ( !create_shader_module( *this, creation.vertex_shader,
                                shader_state->shader_stage_info[ 0 ]) ) {
        ilog_error( "Error creating shader %s\n", creation.debug_name );

        // TODO:
        destroy_shader_state( handle );
        return { 0, 0 };
    }

    if ( !create_shader_module( *this, creation.fragment_shader,
                                shader_state->shader_stage_info[ 1 ]) ) {
        ilog_error( "Error creating shader %s\n", creation.debug_name );

        // TODO:
        destroy_shader_state( handle );
        return { 0, 0 };
    }

    shader_state->debug_name = creation.debug_name;
    shader_state->num_active_shaders = 2;

    set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, ( u64 )shader_state->shader_stage_info[ 0 ].module, creation.debug_name );
    set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, ( u64 )shader_state->shader_stage_info[ 1 ].module, creation.debug_name );

    return handle;

    //ShaderStateHandle handle{ 0,0 };

    //ShaderStateHandle shader = shader_states.obtain_object();
    //if ( shader.is_invalid() ) {
    //    return handle;
    //}

    //ShaderState* shader_state = shader_states.get_cold( shader );
    //VulkanShaderState* vk_shader_state = shader_states.get_hot( shader );

    ////resource_tracker.track_create_resource( ResourceUpdateType::ShaderState, handle.index, creation.name );

    //// For each shader stage, compile them individually.
    //u32 compiled_shaders = 0;

    //shader_state->pipeline_type = PipelineType::Graphics;
    //shader_state->num_active_shaders = 0;

    // TODO(marco): should we keep this around?
    //sizet current_temporary_marker;// = temporary_allocator->get_marker();

    ////StringBuffer name_buffer;
    ////name_buffer.init( 16000, temporary_allocator );

    //// Parse result needs to be always in memory as its used to free descriptor sets.
    ////shader_state->parse_result = ( spirv::ParseResult* )allocator->allocate( sizeof( spirv::ParseResult ), 64 );
    ////memset( shader_state->parse_result, 0, sizeof( spirv::ParseResult ) );

    //u32 broken_stage = u32_max;

    //for ( compiled_shaders = 0; compiled_shaders < creation.num_stages; ++compiled_shaders ) {
    //    const ShaderStageCode& stage = creation.stages[ compiled_shaders ];

    //    // Gives priority to compute: if any is present (and it should not be) then it is not a graphics pipeline.
    //    if ( stage.type == VK_SHADER_STAGE_COMPUTE_BIT ) {
    //        shader_state->graphics_pipeline = false;
    //    }

    //    VkShaderModuleCreateInfo shader_create_info = {
    //        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    //        .flags = 0,
    //        .codeSize = stage.byte_code.size_,
    //        .pCode = stage.byte_code.data };

    //    /*if ( creation.spv_input ) {
    //        shader_create_info.codeSize = stage.code_size;
    //        shader_create_info.pCode = reinterpret_cast< const u32* >( stage.code );
    //    } else {
    //        shader_create_info = compile_shader( stage.code, stage.code_size, stage.type, creation.name );
    //    }*/

    //    // Spir-V file is not generated when there is a compilation error, we can use this to know when compilation is succeded.
    //    if ( shader_create_info.pCode ) {
    //        // Parse the generated Spir-V to obtain specialization constants informations.
    //        //spirv::parse_binary( shader_create_info.pCode, shader_create_info.codeSize, name_buffer, shader_state->parse_result );

    //        // Compile shader module
    //        VkPipelineShaderStageCreateInfo& shader_stage_info = shader_state->shader_stage_info[ compiled_shaders ];
    //        memset( &shader_stage_info, 0, sizeof( VkPipelineShaderStageCreateInfo ) );
    //        shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    //        shader_stage_info.pName = "main";
    //        shader_stage_info.stage = to_vk_shader_stage( stage.type );

    //        // NOTE: this needs to be static because pipeline reference it.
    //        //static VkSpecializationInfo specialization_info;
    //        //static VkSpecializationMapEntry specialization_entries[ spirv::k_max_specialization_constants ];
    //        //static u32 specialization_data[ spirv::k_max_specialization_constants ];

    //        //// Add optional specialization constants.
    //        //if ( shader_state->parse_result->specialization_constants_count ) {

    //        //    specialization_info.mapEntryCount = shader_state->parse_result->specialization_constants_count;
    //        //    // NOTE: we assume specialization constants to either be i32,u32 or floats.
    //        //    specialization_info.dataSize = shader_state->parse_result->specialization_constants_count * sizeof( u32 );
    //        //    specialization_info.pMapEntries = specialization_entries;
    //        //    specialization_info.pData = specialization_data;

    //        //    for ( u32 i = 0; i < shader_state->parse_result->specialization_constants_count; ++i ) {

    //        //        const spirv::SpecializationConstant& specialization_constant = shader_state->parse_result->specialization_constants[ i ];
    //        //        cstring specialization_name = shader_state->parse_result->specialization_names[ i ].name;
    //        //        VkSpecializationMapEntry& specialization_entry = specialization_entries[ i ];

    //        //        if ( strcmp( specialization_name, "SUBGROUP_SIZE" ) == 0 ) {
    //        //            specialization_entry.constantID = specialization_constant.binding;
    //        //            specialization_entry.size = sizeof( u32 );
    //        //            specialization_entry.offset = i * sizeof( u32 );

    //        //            specialization_data[ i ] = subgroup_size;
    //        //        }
    //        //    }

    //        //    shader_stage_info.pSpecializationInfo = &specialization_info;
    //        //}

    //        if ( vkCreateShaderModule( vk_device, &shader_create_info, nullptr, &shader_state->shader_stage_info[ compiled_shaders ].module ) != VK_SUCCESS ) {
    //            broken_stage = compiled_shaders;
    //        }
    //    } else {
    //        broken_stage = compiled_shaders;
    //    }

    //    if ( broken_stage != u32_max ) {
    //        break;
    //    }

    //    switch ( stage.type ) {
    //        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
    //        {
    //            VkRayTracingShaderGroupCreateInfoKHR& shader_group_info = shader_state->shader_group_info[ compiled_shaders ];
    //            memset( &shader_group_info, 0, sizeof( VkRayTracingShaderGroupCreateInfoKHR ) );

    //            shader_group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    //            shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    //            shader_group_info.generalShader = compiled_shaders;
    //            shader_group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

    //            shader_state->graphics_pipeline = false;
    //            shader_state->ray_tracing_pipeline = true;
    //        } break;
    //        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
    //        {
    //            VkRayTracingShaderGroupCreateInfoKHR& shader_group_info = shader_state->shader_group_info[ compiled_shaders ];
    //            memset( &shader_group_info, 0, sizeof( VkRayTracingShaderGroupCreateInfoKHR ) );

    //            shader_group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    //            shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    //            shader_group_info.generalShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.anyHitShader = compiled_shaders;
    //            shader_group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

    //            shader_state->graphics_pipeline = false;
    //            shader_state->ray_tracing_pipeline = true;
    //        } break;
    //        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
    //        {
    //            VkRayTracingShaderGroupCreateInfoKHR& shader_group_info = shader_state->shader_group_info[ compiled_shaders ];
    //            memset( &shader_group_info, 0, sizeof( VkRayTracingShaderGroupCreateInfoKHR ) );

    //            shader_group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    //            shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    //            shader_group_info.generalShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.closestHitShader = compiled_shaders;
    //            shader_group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

    //            shader_state->graphics_pipeline = false;
    //            shader_state->ray_tracing_pipeline = true;
    //        } break;
    //        case VK_SHADER_STAGE_MISS_BIT_KHR:
    //        {
    //            VkRayTracingShaderGroupCreateInfoKHR& shader_group_info = shader_state->shader_group_info[ compiled_shaders ];
    //            memset( &shader_group_info, 0, sizeof( VkRayTracingShaderGroupCreateInfoKHR ) );

    //            shader_group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    //            shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    //            shader_group_info.generalShader = compiled_shaders;
    //            shader_group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
    //            shader_group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

    //            shader_state->graphics_pipeline = false;
    //            shader_state->ray_tracing_pipeline = true;
    //        } break;
    //    }

    //    //set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, ( u64 )shader_state->shader_stage_info[ compiled_shaders ].module, creation.name );
    //}
    //// Not needed anymore - temp allocator freed at the end.
    ////name_buffer.shutdown();
    ////temporary_allocator->free_marker( current_temporary_marker );

    //bool creation_failed = compiled_shaders != creation.num_stages;
    //if ( !creation_failed ) {
    //    shader_state->num_active_shaders = compiled_shaders;
    //    shader_state->debug_name = creation.debug_name;
    //}

    //if ( creation_failed ) {
    //    destroy_shader_state( handle );
    //    handle = { 0, k_invalid_generation };

    //    /*if ( !creation.spv_input ) {
    //        const ShaderStage& stage = creation.stages[ broken_stage ];
    //        dump_shader_code( name_buffer, stage.code, stage.type, creation.name );
    //    }*/
    //}

    return handle;
}

static bool create_shader_module( GpuDevice& gpu, const ShaderStageCode& shader, VkPipelineShaderStageCreateInfo& out_shader_stage ) {

    VkShaderModuleCreateInfo shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .flags = 0,
        .codeSize = shader.byte_code.size,
        .pCode = shader.byte_code.data };

    // TODO:
    // Missing specialization constants

    out_shader_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = to_vk_shader_stage( shader.type ),
        .module = nullptr,
        .pName = "main",
        .pSpecializationInfo = nullptr  // TODO
    };

    if ( vkCreateShaderModule( gpu.vk_device, &shader_create_info, nullptr, &out_shader_stage.module ) != VK_SUCCESS ) {
        return false;
    }

    return true;
}

ShaderStateHandle GpuDevice::create_compute_shader_state( const ComputeShaderStateCreation& creation ) {
    ShaderStateHandle handle = shader_states.obtain_object();

    if ( handle.is_invalid() ) {
        return handle;
    }

    ShaderState* shader_state = shader_states.get_cold( handle );
    VulkanShaderState* vk_shader_state = shader_states.get_hot( handle );

    shader_state->pipeline_type = PipelineType::Compute;
    shader_state->num_active_shaders = 0;

    VkPipelineShaderStageCreateInfo shader_stage_create_info{};

    if ( !create_shader_module( *this, creation.compute_shader,
                                shader_stage_create_info ) ) {
        ilog_error( "Error creating shader %s\n", creation.debug_name );

        // TODO:
        destroy_shader_state( handle );
        return { 0, 0 };
    }

    shader_state->debug_name = creation.debug_name;
    shader_state->num_active_shaders = 1;

    set_resource_name( VK_OBJECT_TYPE_SHADER_MODULE, ( u64 )shader_stage_create_info.module, creation.debug_name );

    // TODO: allocation
    // Allocate one shader stage info
    shader_state->shader_stage_info = ( VkPipelineShaderStageCreateInfo* )ialloc( sizeof( VkPipelineShaderStageCreateInfo ), &shader_info_allocators[ PipelineType::Compute ] );
    *shader_state->shader_stage_info = shader_stage_create_info;
    shader_state->shader_group_info = nullptr;

    return handle;
}

PipelineHandle GpuDevice::create_graphics_pipeline( const GraphicsPipelineCreation& creation ) {
    PipelineHandle handle = pipelines.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    Pipeline* pipeline = pipelines.get_cold( handle );
    VulkanPipeline* vk_pipeline = pipelines.get_hot( handle );
    ShaderState* shader_state_data = shader_states.get_cold( creation.shader );

    pipeline->shader_state = creation.shader;

    VkDescriptorSetLayout vk_layouts[ k_max_descriptor_set_layouts ];

    u32 num_active_layouts = ( u32 )creation.descriptor_set_layouts.size;// shader_state_data->parse_result->set_count;

    // Create VkPipelineLayout
    for ( u32 l = 0; l < num_active_layouts; ++l ) {

        DescriptorSetLayout* descriptor_set_layout = descriptor_set_layouts.get_cold( creation.descriptor_set_layouts[ l ] );
        pipeline->descriptor_set_layout[ l ] = descriptor_set_layout;

        VulkanDescriptorSetLayout* vk_dsl = descriptor_set_layouts.get_hot( creation.descriptor_set_layouts[ l ] );
        vk_layouts[ l ] = vk_dsl->vk_descriptor_set_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_info.pSetLayouts = vk_layouts;
    pipeline_layout_info.setLayoutCount = num_active_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;

    //VkPushConstantRange push_constant;

    /*if ( shader_state_data->parse_result->push_constants_stride ) {

        push_constant.offset = 0;
        push_constant.size = shader_state_data->parse_result->push_constants_stride;
        push_constant.stageFlags = VK_SHADER_STAGE_ALL;

        pipeline_layout_info.pPushConstantRanges = &push_constant;
        pipeline_layout_info.pushConstantRangeCount = 1;
    }*/

    // Create the Vulkan compute pipeline
    VkPipelineLayout pipeline_layout;
    vkcheck( vkCreatePipelineLayout( vk_device, &pipeline_layout_info, vk_allocation_callbacks, &pipeline_layout ) );

    // Cache pipeline layout
    vk_pipeline->vk_pipeline_layout = pipeline_layout;
    pipeline->num_active_layouts = num_active_layouts;

    VkGraphicsPipelineCreateInfo pipeline_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    //pipeline_info.flags = creation.flags;

    //// Shader stage
    pipeline_info.pStages = shader_state_data->shader_stage_info;
    pipeline_info.stageCount = shader_state_data->num_active_shaders;
    //// PipelineLayout
    pipeline_info.layout = pipeline_layout;

    //// Vertex input
    VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // Vertex attributes.
    VkVertexInputAttributeDescription vertex_attributes[ 8 ];
    const u32 num_vertex_attributes = (u32)creation.vertex_input.vertex_attributes.size;
    if ( num_vertex_attributes ) {

        for ( u32 i = 0; i < num_vertex_attributes; ++i ) {
            const VertexAttribute& vertex_attribute = creation.vertex_input.vertex_attributes[ i ];
            vertex_attributes[ i ] = { vertex_attribute.location, vertex_attribute.binding, to_vk_vertex_format( vertex_attribute.format ), vertex_attribute.offset };
        }

        vertex_input_info.vertexAttributeDescriptionCount = num_vertex_attributes;
        vertex_input_info.pVertexAttributeDescriptions = vertex_attributes;
    } else {
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr;
    }
    // Vertex bindings
    VkVertexInputBindingDescription vertex_bindings[ 8 ];
    const u32 num_vertex_streams = (u32)creation.vertex_input.vertex_streams.size;
    if ( num_vertex_streams ) {
        vertex_input_info.vertexBindingDescriptionCount = num_vertex_streams;

        for ( u32 i = 0; i < num_vertex_streams; ++i ) {
            const VertexStream& vertex_stream = creation.vertex_input.vertex_streams[ i ];
            VkVertexInputRate vertex_rate = vertex_stream.input_rate == VertexInputRate::PerVertex ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX : VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE;
            vertex_bindings[ i ] = { vertex_stream.binding, vertex_stream.stride, vertex_rate };
        }
        vertex_input_info.pVertexBindingDescriptions = vertex_bindings;
    } else {
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr;
    }

    pipeline_info.pVertexInputState = &vertex_input_info;

    //// Input Assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    //input_assembly.topology = creation.topology;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    pipeline_info.pInputAssemblyState = &input_assembly;

    //// Color Blending
    VkPipelineColorBlendAttachmentState color_blend_attachment[ 8 ];
    const u32 num_active_blend_states = (u32)creation.blend_state.blend_states.size;
    if ( num_active_blend_states ) {
        //iassertm( creation.blend_state.active_states == creation.render_pass.num_color_formats, "Blend states (count: %u) mismatch with output targets (count %u)!If blend states are active, they must be defined for all outputs", creation.blend_state.active_states, creation.render_pass.num_color_formats );
        for ( size_t i = 0; i < num_active_blend_states; i++ ) {
            const BlendState& blend_state = creation.blend_state.blend_states[ i ];

            color_blend_attachment[ i ].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachment[ i ].blendEnable = blend_state.blend_disabled ? VK_FALSE : VK_TRUE;
            color_blend_attachment[ i ].srcColorBlendFactor = to_vk_blend_factor(blend_state.source_color);
            color_blend_attachment[ i ].dstColorBlendFactor = to_vk_blend_factor(blend_state.destination_color);
            color_blend_attachment[ i ].colorBlendOp = to_vk_blend_operation(blend_state.color_operation);

            if ( blend_state.separate_blend ) {
                color_blend_attachment[ i ].srcAlphaBlendFactor = to_vk_blend_factor(blend_state.source_alpha);
                color_blend_attachment[ i ].dstAlphaBlendFactor = to_vk_blend_factor(blend_state.destination_alpha);
                color_blend_attachment[ i ].alphaBlendOp = to_vk_blend_operation(blend_state.alpha_operation);
            } else {
                color_blend_attachment[ i ].srcAlphaBlendFactor = to_vk_blend_factor(blend_state.source_color);
                color_blend_attachment[ i ].dstAlphaBlendFactor = to_vk_blend_factor(blend_state.destination_color);
                color_blend_attachment[ i ].alphaBlendOp = to_vk_blend_operation(blend_state.color_operation);
            }
        }
    } else {
        // Default non blended state
        for ( u32 i = 0; i < 8; ++i ) {
            color_blend_attachment[ i ] = {};
            color_blend_attachment[ i ].blendEnable = VK_FALSE;
            color_blend_attachment[ i ].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
    }

    VkPipelineColorBlendStateCreateInfo color_blending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    //color_blending.attachmentCount = creation.blend_state.active_states ? creation.blend_state.active_states : creation.render_pass.num_color_formats;
    color_blending.attachmentCount = num_active_blend_states ? num_active_blend_states : (u32)creation.color_formats.size;
    color_blending.pAttachments = color_blend_attachment;
    color_blending.blendConstants[ 0 ] = 0.0f; // Optional
    color_blending.blendConstants[ 1 ] = 0.0f; // Optional
    color_blending.blendConstants[ 2 ] = 0.0f; // Optional
    color_blending.blendConstants[ 3 ] = 0.0f; // Optional

    pipeline_info.pColorBlendState = &color_blending;

    //// Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    depth_stencil.depthWriteEnable = creation.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.stencilTestEnable = creation.depth_stencil.stencil_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthTestEnable = creation.depth_stencil.depth_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = to_vk_compare_operation( creation.depth_stencil.depth_comparison );
    if ( creation.depth_stencil.stencil_enable ) {
        // TODO: add stencil
        iassert( false );
    }

    pipeline_info.pDepthStencilState = &depth_stencil;

    //// Multisample
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    pipeline_info.pMultisampleState = &multisampling;

    //// Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = to_vk_polygon_mode( creation.rasterization.fill );
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = to_vk_cull_mode( creation.rasterization.cull_mode );
    rasterizer.frontFace = to_vk_front_face( creation.rasterization.front );
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    pipeline_info.pRasterizationState = &rasterizer;

    //// Tessellation
    pipeline_info.pTessellationState;


    //// Viewport state
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = ( float )swapchain_width;
    viewport.height = ( float )swapchain_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = { swapchain_width, swapchain_height };

    VkPipelineViewportStateCreateInfo viewport_state{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    pipeline_info.pViewportState = &viewport_state;

    //// Render Pass
    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    //if ( dynamic_rendering_extension_present )
    {
        VkFormat color_formats[ k_max_image_outputs ];
        for (u32 i = 0; i < creation.color_formats.size; ++i ) {
            color_formats[ i ] = to_vk_format( creation.color_formats[ i ] );
        }

        pipeline_rendering_create_info.viewMask = 0;
        pipeline_rendering_create_info.colorAttachmentCount = (u32)creation.color_formats.size;
        pipeline_rendering_create_info.pColorAttachmentFormats = color_formats;

        pipeline_rendering_create_info.depthAttachmentFormat = to_vk_format( creation.depth_format );
        // TODO:
        pipeline_rendering_create_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        pipeline_info.pNext = &pipeline_rendering_create_info;
    } /*else {
        pipeline_info.renderPass = get_vulkan_render_pass( creation.render_pass, creation.name );
    }*/

    //// Dynamic states
    VkPipelineDynamicStateCreateInfo dynamic_state{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

    VkDynamicState dynamic_states[ 3 ] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    /*if ( fragment_shading_rate_present ) {
        dynamic_states[ 2 ] = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
        dynamic_state.dynamicStateCount = ArraySize( dynamic_states );
    } else*/
    {
        dynamic_state.dynamicStateCount = 2;
    }

    dynamic_state.pDynamicStates = dynamic_states;

    pipeline_info.pDynamicState = &dynamic_state;

    // TODO:
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    vkcheck( vkCreateGraphicsPipelines( vk_device, pipeline_cache, 1, &pipeline_info, vk_allocation_callbacks, &vk_pipeline->vk_pipeline ) );

    vk_pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;

    set_resource_name( VK_OBJECT_TYPE_PIPELINE, ( u64 )vk_pipeline->vk_pipeline, creation.debug_name );

    return handle;
}

PipelineHandle GpuDevice::create_compute_pipeline( const ComputePipelineCreation& creation ) {

    PipelineHandle handle = pipelines.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::Pipeline, handle.index, creation.name );

    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    VkPipelineCacheCreateInfo pipeline_cache_create_info{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };

    //bool cache_exists = file_exists( cache_path );
    //if ( cache_path != nullptr && cache_exists ) {
    //    FileReadResult read_result = file_read_binary( cache_path, allocator );

    //    VkPipelineCacheHeaderVersionOne* cache_header = ( VkPipelineCacheHeaderVersionOne* )read_result.data;

    //    if ( cache_header->deviceID == vulkan_physical_properties.deviceID &&
    //         cache_header->vendorID == vulkan_physical_properties.vendorID &&
    //         memcmp( cache_header->pipelineCacheUUID, vulkan_physical_properties.pipelineCacheUUID, VK_UUID_SIZE ) == 0 )
    //    {
    //        pipeline_cache_create_info.initialDataSize = read_result.size;
    //        pipeline_cache_create_info.pInitialData = read_result.data;
    //    } else
    //    {
    //        cache_exists = false;
    //    }

    //    check( vkCreatePipelineCache( vulkan_device, &pipeline_cache_create_info, vulkan_allocation_callbacks, &pipeline_cache ) );

    //    allocator->deallocate( read_result.data );
    //} else {
    //    check( vkCreatePipelineCache( vulkan_device, &pipeline_cache_create_info, vulkan_allocation_callbacks, &pipeline_cache ) );
    //}

    //ShaderStateHandle shader_state = create_shader_state( creation.shaders );
    //if ( shader_state.index == k_invalid_index ) {
    //    // Shader did not compile.
    //    pipelines.release_resource( handle.index );
    //    handle.index = k_invalid_index;

    //    return handle;
    //}

    // Now that shaders have compiled we can create the pipeline.
    Pipeline* pipeline = pipelines.get_cold( handle );
    VulkanPipeline* vk_pipeline = pipelines.get_hot( handle );

    ShaderState* shader_state_data = shader_states.get_cold( creation.shader );

    pipeline->shader_state = creation.shader;

    VkDescriptorSetLayout vk_layouts[ k_max_descriptor_set_layouts ];

    u32 num_active_layouts = (u32)creation.descriptor_set_layouts.size;// shader_state_data->parse_result->set_count;

    // Create VkPipelineLayout
    for ( u32 l = 0; l < num_active_layouts; ++l ) {

        DescriptorSetLayout* descriptor_set_layout = descriptor_set_layouts.get_cold( creation.descriptor_set_layouts[ l ] );
        pipeline->descriptor_set_layout[ l ] = descriptor_set_layout;

#if 0
        DescriptorBinding* descriptor_bindings = descriptor_set_layout->bindings;
        ilog( "Layout debug for pipeline %s\n", creation.name );
        for ( u32 b = 0; b < descriptor_set_layout->num_bindings; ++b ) {
            DescriptorBinding& binding = descriptor_bindings[ b ];
            ilog( "%s (%d, %d)\n", binding.name, binding.set, binding.index );
        }
#endif
        VulkanDescriptorSetLayout* vk_dsl = descriptor_set_layouts.get_hot( creation.descriptor_set_layouts[ l ] );
        vk_layouts[ l ] = vk_dsl->vk_descriptor_set_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_info.pSetLayouts = vk_layouts;
    pipeline_layout_info.setLayoutCount = num_active_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;

    //VkPushConstantRange push_constant;

    /*if ( shader_state_data->parse_result->push_constants_stride ) {

        push_constant.offset = 0;
        push_constant.size = shader_state_data->parse_result->push_constants_stride;
        push_constant.stageFlags = VK_SHADER_STAGE_ALL;

        pipeline_layout_info.pPushConstantRanges = &push_constant;
        pipeline_layout_info.pushConstantRangeCount = 1;
    }*/

    // Create the Vulkan compute pipeline
    VkPipelineLayout pipeline_layout;
    vkcheck( vkCreatePipelineLayout( vk_device, &pipeline_layout_info, vk_allocation_callbacks, &pipeline_layout ) );

    VkComputePipelineCreateInfo pipeline_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    pipeline_info.stage = shader_state_data->shader_stage_info[ 0 ];
    pipeline_info.layout = pipeline_layout;

    vkcheck( vkCreateComputePipelines( vk_device, pipeline_cache, 1, &pipeline_info, vk_allocation_callbacks, &vk_pipeline->vk_pipeline ) );

    // Cache pipeline layout
    vk_pipeline->vk_pipeline_layout = pipeline_layout;
    vk_pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE;

    pipeline->num_active_layouts = num_active_layouts;

    // TODO: restore pipeline cache
    /*if ( cache_path != nullptr && !cache_exists ) {
        sizet cache_data_size = 0;
        check( vkGetPipelineCacheData( vulkan_device, pipeline_cache, &cache_data_size, nullptr ) );

        void* cache_data = allocator->allocate( cache_data_size, 64 );
        check( vkGetPipelineCacheData( vulkan_device, pipeline_cache, &cache_data_size, cache_data ) );

        file_write_binary( cache_path, cache_data, cache_data_size );

        allocator->deallocate( cache_data );
    }*/

    vkDestroyPipelineCache( vk_device, pipeline_cache, vk_allocation_callbacks );

    set_resource_name( VK_OBJECT_TYPE_PIPELINE, ( u64 )vk_pipeline->vk_pipeline, creation.debug_name );

    return handle;
}

SamplerHandle GpuDevice::create_sampler( const SamplerCreation& creation ) {

    SamplerHandle handle = samplers.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::Sampler, handle.index, creation.name );

    Sampler* sampler = samplers.get_cold( handle );
    VulkanSampler* vk_sampler = samplers.get_hot( handle );

    sampler->address_mode_u = to_vk_address_mode( creation.address_mode_u );
    sampler->address_mode_v = to_vk_address_mode( creation.address_mode_v );
    sampler->address_mode_w = to_vk_address_mode( creation.address_mode_w );
    sampler->min_filter = to_vk_filter( creation.min_filter );
    sampler->mag_filter = to_vk_filter( creation.mag_filter );
    sampler->mip_filter = to_vk_mipmap( creation.mip_filter );
    sampler->name = creation.debug_name;
    //sampler->reduction_mode = creation.reduction_mode;

    VkSamplerCreateInfo create_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    create_info.addressModeU = to_vk_address_mode( creation.address_mode_u );
    create_info.addressModeV = to_vk_address_mode( creation.address_mode_v );
    create_info.addressModeW = to_vk_address_mode( creation.address_mode_w );
    create_info.minFilter = to_vk_filter( creation.min_filter );
    create_info.magFilter = to_vk_filter( creation.mag_filter );
    create_info.mipmapMode = to_vk_mipmap( creation.mip_filter );
    create_info.anisotropyEnable = 0;
    create_info.compareEnable = 0;
    create_info.unnormalizedCoordinates = 0;
    create_info.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    create_info.minLod = 0;
    create_info.maxLod = 16;
    // TODO:
    /*float                   mipLodBias;
    float                   maxAnisotropy;
    VkCompareOp             compareOp;
    VkBorderColor           borderColor;
    VkBool32                unnormalizedCoordinates;*/

    //VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };
    //// Add optional reduction mode.
    //if ( creation.reduction_mode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT ) {
    //    createInfoReduction.reductionMode = creation.reduction_mode;

    //    create_info.pNext = &createInfoReduction;
    //}

    vkCreateSampler( vk_device, &create_info, vk_allocation_callbacks, &vk_sampler->vk_sampler );

    set_resource_name( VK_OBJECT_TYPE_SAMPLER, ( u64 )vk_sampler->vk_sampler, creation.debug_name );

    return handle;
}

DescriptorSetBindingsPools::Enum get_binding_allocator_index( u32 num_bindings ) {

    if ( num_bindings <= 2 ) {
        return DescriptorSetBindingsPools::_2;
    }
    if ( num_bindings <= 4 ) {
        return DescriptorSetBindingsPools::_4;
    }
    if ( num_bindings <= 8 ) {
        return DescriptorSetBindingsPools::_8;
    }
    if ( num_bindings <= 16 ) {
        return DescriptorSetBindingsPools::_16;
    }

    return DescriptorSetBindingsPools::_32;
}

DescriptorSetLayoutHandle GpuDevice::create_descriptor_set_layout( const DescriptorSetLayoutCreation& creation ) {
    DescriptorSetLayoutHandle handle = descriptor_set_layouts.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::DescriptorSetLayout, handle.index, creation.name );
    DescriptorSetLayout* descriptor_set_layout = descriptor_set_layouts.get_cold( handle );
    VulkanDescriptorSetLayout* vk_descriptor_set_layout = descriptor_set_layouts.get_hot( handle );

    const u32 num_bindings = (u32)creation.bindings.size;
    u16 max_binding = 0;
    for ( u32 r = 0; r < num_bindings; ++r ) {
        const DescriptorBinding& input_binding = creation.bindings[ r ];
        max_binding = max_binding > input_binding.start ? max_binding : input_binding.start;// idra_max( max_binding, input_binding.index );
    }
    max_binding += 1;

    // Create flattened binding list
    descriptor_set_layout->num_bindings = ( u16 )num_bindings;
    descriptor_set_layout->num_dynamic_bindings = ( u16 )creation.dynamic_buffer_bindings.size;
    const u32 total_bindings = num_bindings + (u32)creation.dynamic_buffer_bindings.size;

    DescriptorSetBindingsPools::Enum pool_index = get_binding_allocator_index( total_bindings );
    Allocator* binding_allocator = &descriptor_set_bindings_allocators[pool_index];
    // Remap to currently allocated bindings for the pool.
    // Ex: if bindings is 1, but the pool is the first, it will allocate 2 bindings.
    // Modifying this value will allocate the expected count from the pool.
    const u32 allocated_bindings = DescriptorSetBindingsPools::s_counts[ pool_index ];

    u8* memory = iallocm( ( ( sizeof( VkDescriptorSetLayoutBinding ) ) * allocated_bindings ), binding_allocator);
    iassert( memory );
    descriptor_set_layout->vk_binding = ( VkDescriptorSetLayoutBinding* )( memory );
    descriptor_set_layout->handle = handle;
    descriptor_set_layout->bindless = 0;
    descriptor_set_layout->dynamic = creation.dynamic_buffer_bindings.size ? 1 : 0;

    u32 used_bindings = 0;

    for ( u32 r = 0; r < num_bindings; ++r ) {
        const DescriptorBinding& input_binding = creation.bindings[ r ];

        VkDescriptorSetLayoutBinding& vk_binding = descriptor_set_layout->vk_binding[ used_bindings ];
        ++used_bindings;

        vk_binding.binding = input_binding.start;
        vk_binding.descriptorType = to_vk_descriptor_type( input_binding.type );
        vk_binding.descriptorCount = input_binding.count;

        // TODO:
        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Add dynamic buffer binding
    for ( u32 r = 0; r < (u32)creation.dynamic_buffer_bindings.size; ++r ) {
        VkDescriptorSetLayoutBinding& vk_binding = descriptor_set_layout->vk_binding[ used_bindings ];
        ++used_bindings;

        vk_binding.binding = creation.dynamic_buffer_bindings[ r ];
        vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        vk_binding.descriptorCount = 1;
        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layout_info.bindingCount = used_bindings;
    layout_info.pBindings = descriptor_set_layout->vk_binding;

    vkCreateDescriptorSetLayout( vk_device, &layout_info, vk_allocation_callbacks, &vk_descriptor_set_layout->vk_descriptor_set_layout );

    return handle;
}

DescriptorSetLayoutHandle GpuDevice::create_bindless_descriptor_set_layout(
    const DescriptorSetLayoutCreation& creation ) {

    DescriptorSetLayoutHandle handle = descriptor_set_layouts.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::DescriptorSetLayout, handle.index, creation.name );
    DescriptorSetLayout* descriptor_set_layout = descriptor_set_layouts.get_cold( handle );
    VulkanDescriptorSetLayout* vk_descriptor_set_layout = descriptor_set_layouts.get_hot( handle );

    const u32 num_bindings = ( u32 )creation.bindings.size;
    u16 max_binding = 0;
    for ( u32 r = 0; r < num_bindings; ++r ) {
        const DescriptorBinding& input_binding = creation.bindings[ r ];
        max_binding = max_binding > input_binding.start ? max_binding : input_binding.start;// idra_max( max_binding, input_binding.index );
    }
    max_binding += 1;

    // Create flattened binding list
    // TODO: allocation
    descriptor_set_layout->num_bindings = ( u16 )num_bindings;
    descriptor_set_layout->num_dynamic_bindings = ( u16 )creation.dynamic_buffer_bindings.size;
    const u32 total_bindings = num_bindings + ( u32 )creation.dynamic_buffer_bindings.size;

    DescriptorSetBindingsPools::Enum pool_index = get_binding_allocator_index( total_bindings );
    Allocator* binding_allocator = &descriptor_set_bindings_allocators[ pool_index ];
    // Remap to currently allocated bindings for the pool.
    // Ex: if bindings is 1, but the pool is the first, it will allocate 2 bindings.
    // Modifying this value will allocate the expected count from the pool.
    const u32 allocated_bindings = DescriptorSetBindingsPools::s_counts[ pool_index ];

    u8* memory = iallocm( ( ( sizeof( VkDescriptorSetLayoutBinding ) ) * allocated_bindings ), binding_allocator );
    descriptor_set_layout->vk_binding = ( VkDescriptorSetLayoutBinding* )( memory );
    descriptor_set_layout->handle = handle;
    descriptor_set_layout->bindless = 1;
    descriptor_set_layout->dynamic = creation.dynamic_buffer_bindings.size ? 1 : 0;

    u32 used_bindings = 0;

    for ( u32 r = 0; r < num_bindings; ++r ) {
        const DescriptorBinding& input_binding = creation.bindings[ r ];

        VkDescriptorSetLayoutBinding& vk_binding = descriptor_set_layout->vk_binding[ used_bindings ];
        ++used_bindings;

        vk_binding.binding = input_binding.start;
        vk_binding.descriptorType = to_vk_descriptor_type( input_binding.type );
        vk_binding.descriptorCount = input_binding.count;

        // TODO:
        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Add dynamic buffer binding
    for ( u32 r = 0; r < ( u32 )creation.dynamic_buffer_bindings.size; ++r ) {
        VkDescriptorSetLayoutBinding& vk_binding = descriptor_set_layout->vk_binding[ used_bindings ];
        ++used_bindings;

        vk_binding.binding = creation.dynamic_buffer_bindings[ r ];
        vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        vk_binding.descriptorCount = 1;
        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layout_info.bindingCount = used_bindings;
    layout_info.pBindings = descriptor_set_layout->vk_binding;

    // Needs update after bind flag.
    layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

    // TODO: reenable variable descriptor count
    // Binding flags
    VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;//VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
    VkDescriptorBindingFlags binding_flags[ 16 ];

    for ( u32 r = 0; r < num_bindings; ++r ) {
        binding_flags[ r ] = bindless_flags;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
    extended_info.bindingCount = used_bindings;
    extended_info.pBindingFlags = binding_flags;

    layout_info.pNext = &extended_info;
    vkCreateDescriptorSetLayout( vk_device, &layout_info, vk_allocation_callbacks, &vk_descriptor_set_layout->vk_descriptor_set_layout );

    return handle;
}

struct DescriptorSortingData {
    u16             binding_point;
    u16             resource_index;
};

static int sorting_descriptor_func( const void* a, const void* b ) {
    const DescriptorSortingData* da = ( const DescriptorSortingData* )a;
    const DescriptorSortingData* db = ( const DescriptorSortingData* )b;

    if ( da->binding_point < db->binding_point )
        return -1;
    else if ( da->binding_point > db->binding_point )
        return 1;
    return 0;
}

DescriptorSetHandle GpuDevice::create_descriptor_set( const DescriptorSetCreation& creation ) {
    DescriptorSetHandle handle = descriptor_sets.obtain_object();
    if ( handle.is_invalid() ) {
        return handle;
    }

    //resource_tracker.track_create_resource( ResourceUpdateType::DescriptorSet, handle.index, creation.name );

    DescriptorSet* descriptor_set = descriptor_sets.get_cold( handle );
    VulkanDescriptorSet* vk_descriptor_set = descriptor_sets.get_hot( handle );
    const DescriptorSetLayout* descriptor_set_layout = descriptor_set_layouts.get_cold( creation.layout );
    const VulkanDescriptorSetLayout* vk_descriptor_set_layout = descriptor_set_layouts.get_hot( creation.layout );

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.descriptorPool = descriptor_set_layout->bindless ? vk_bindless_descriptor_pool : vk_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &vk_descriptor_set_layout->vk_descriptor_set_layout;

    descriptor_set->name = creation.debug_name;

    if ( descriptor_set_layout->bindless ) {
        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
        u32 max_binding = k_max_bindless_resources - 1;
        count_info.descriptorSetCount = 1;
        // This number is the max allocatable count
        count_info.pDescriptorCounts = &max_binding;
        alloc_info.pNext = &count_info;
        vkcheck( vkAllocateDescriptorSets( vk_device, &alloc_info, &vk_descriptor_set->vk_descriptor_set ) );
    } else {
        vkcheck( vkAllocateDescriptorSets( vk_device, &alloc_info, &vk_descriptor_set->vk_descriptor_set ) );
    }

    VkWriteDescriptorSet descriptors_to_modify[ k_max_bindings_per_list ];

    VkDescriptorBufferInfo buffer_info[ k_max_bindings_per_list ];
    VkDescriptorImageInfo textures_info[ k_max_bindings_per_list ];
    VkDescriptorImageInfo image_info[ k_max_bindings_per_list ];

    u32 written_descriptors = 0;

    VulkanSampler* sampler = samplers.get_hot( default_sampler );

    for ( u32 i = 0; i < creation.textures.size; ++i) {

        Texture* texture_data = textures.get_cold( creation.textures[ i ].texture );
        VulkanTexture* vk_texture = textures.get_hot( creation.textures[ i ].texture );

        textures_info[ i ].imageView = vk_texture->vk_image_view;

        if ( texture_data->sampler.is_valid() ) {
            VulkanSampler* sampler = samplers.get_hot( texture_data->sampler );
            textures_info[ i ].sampler = sampler->vk_sampler;
        }
        else {
            textures_info[ i ].sampler = sampler->vk_sampler;
        }

        // TODO:
        //if ( gpu.synchronization2_extension_present ) {
        textures_info[ i ].imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

        descriptors_to_modify[ written_descriptors ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptors_to_modify[ written_descriptors ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptors_to_modify[ written_descriptors ].dstSet = vk_descriptor_set->vk_descriptor_set;
        descriptors_to_modify[ written_descriptors ].dstBinding = creation.textures[ i ].binding;
        descriptors_to_modify[ written_descriptors ].dstArrayElement = 0;
        descriptors_to_modify[ written_descriptors ].descriptorCount = 1;
        descriptors_to_modify[ written_descriptors ].pImageInfo = &textures_info[ i ];
        ++written_descriptors;
    }

    for ( u32 i = 0; i < creation.images.size; ++i ) {


        image_info[ i ].sampler = nullptr;
        image_info[ i ].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        Texture* texture_data = textures.get_cold( creation.images[ i ].texture );
        VulkanTexture* vk_texture = textures.get_hot( creation.images[ i ].texture );

        image_info[ i ].imageView = vk_texture->vk_image_view;

        descriptors_to_modify[ written_descriptors ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptors_to_modify[ written_descriptors ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptors_to_modify[ written_descriptors ].dstSet = vk_descriptor_set->vk_descriptor_set;
        descriptors_to_modify[ written_descriptors ].dstBinding = creation.images[ i ].binding;
        descriptors_to_modify[ written_descriptors ].dstArrayElement = 0;
        descriptors_to_modify[ written_descriptors ].descriptorCount = 1;
        descriptors_to_modify[ written_descriptors ].pImageInfo = &image_info[ i ];
        ++written_descriptors;
    }

    for ( u32 i = 0; i < creation.buffers.size; ++i ) {

        Buffer* buffer = buffers.get_cold( creation.buffers[ i ].buffer );
        VulkanBuffer* vk_buffer = buffers.get_hot( creation.buffers[ i ].buffer );
        buffer_info[ i ].buffer = vk_buffer->vk_buffer;
        buffer_info[ i ].offset = 0;
        buffer_info[ i ].range = buffer->size;

        descriptors_to_modify[ written_descriptors ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptors_to_modify[ written_descriptors ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptors_to_modify[ written_descriptors ].dstSet = vk_descriptor_set->vk_descriptor_set;
        descriptors_to_modify[ written_descriptors ].dstBinding = creation.buffers[ i ].binding;
        descriptors_to_modify[ written_descriptors ].dstArrayElement = 0;
        descriptors_to_modify[ written_descriptors ].descriptorCount = 1;
        descriptors_to_modify[ written_descriptors ].pBufferInfo = &buffer_info[ i ];
        ++written_descriptors;
    }

    for ( u32 i = 0; i < creation.ssbos.size; ++i ) {

        Buffer* buffer = buffers.get_cold( creation.ssbos[ i ].buffer );
        VulkanBuffer* vk_buffer = buffers.get_hot( creation.ssbos[ i ].buffer );
        buffer_info[ i ].buffer = vk_buffer->vk_buffer;
        buffer_info[ i ].offset = 0;
        buffer_info[ i ].range = buffer->size;

        descriptors_to_modify[ written_descriptors ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptors_to_modify[ written_descriptors ].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptors_to_modify[ written_descriptors ].dstSet = vk_descriptor_set->vk_descriptor_set;
        descriptors_to_modify[ written_descriptors ].dstBinding = creation.ssbos[ i ].binding;
        descriptors_to_modify[ written_descriptors ].dstArrayElement = 0;
        descriptors_to_modify[ written_descriptors ].descriptorCount = 1;
        descriptors_to_modify[ written_descriptors ].pBufferInfo = &buffer_info[ i ];
        ++written_descriptors;
    }

    // Add dynamic buffer descriptor
    for ( u32 d = 0; d < ( u32 )creation.dynamic_buffer_bindings.size; ++d ) {
        VulkanBuffer* buffer = buffers.get_hot( dynamic_buffer );

        const u32 i = (u32)(creation.buffers.size + creation.ssbos.size);
        buffer_info[ i ].buffer = buffer->vk_buffer;
        buffer_info[ i ].offset = 0;
        buffer_info[ i ].range = creation.dynamic_buffer_bindings[ d ].size;

        descriptors_to_modify[ written_descriptors ] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptors_to_modify[ written_descriptors ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptors_to_modify[ written_descriptors ].dstSet = vk_descriptor_set->vk_descriptor_set;
        descriptors_to_modify[ written_descriptors ].dstBinding = creation.dynamic_buffer_bindings[ d ].binding;
        descriptors_to_modify[ written_descriptors ].dstArrayElement = 0;
        descriptors_to_modify[ written_descriptors ].descriptorCount = 1;
        descriptors_to_modify[ written_descriptors ].pBufferInfo = &buffer_info[ i ];
        ++written_descriptors;
    }

    // Actually modify the descriptors
    vkUpdateDescriptorSets( vk_device, written_descriptors, descriptors_to_modify, 0, nullptr );

    return handle;
}

void GpuDevice::destroy_buffer( BufferHandle buffer ) {
    if ( buffer.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::Buffer, buffer.index );

        resource_deletion_queue.push( { {buffer.index, buffer.generation}, current_frame, ResourceUpdateType::Buffer});
    } else {
        ilog_debug( "Graphics error: trying to free invalid Buffer %u\n", buffer.index );
    }
}

void GpuDevice::destroy_texture( TextureHandle texture ) {
    if ( texture.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::Texture, texture.index );

        // Do not add textures to deletion queue, textures will be deleted after bindless descriptor is updated.
        // TODO: handle non bindless textures ?
        //resource_deletion_queue.push( { {texture.index, texture.generation}, current_frame, ResourceUpdateType::Texture } );
        texture_to_update_bindless.push( { texture, current_frame, 1 } );
    } else {
        ilog_debug( "Graphics error: trying to free invalid Texture %u\n", texture.index );
    }
}

void GpuDevice::destroy_pipeline( PipelineHandle pipeline ) {
    if ( pipeline.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::Pipeline, pipeline.index );

        resource_deletion_queue.push( { {pipeline.index, pipeline.generation}, current_frame, ResourceUpdateType::Pipeline } );
        // Shader state creation is handled internally when creating a pipeline, thus add this to track correctly.
        Pipeline* v_pipeline = pipelines.get_cold( pipeline );

        /*ShaderState* shader_state_data = access_shader_state( v_pipeline->shader_state );
        for ( u32 l = 0; l < shader_state_data->parse_result->set_count; ++l ) {
            if ( v_pipeline->descriptor_set_layout_handles[ l ].index != k_invalid_index ) {
                destroy_descriptor_set_layout( v_pipeline->descriptor_set_layout_handles[ l ] );
            }
        }*/

        /*if ( shader_state_data->ray_tracing_pipeline ) {
            destroy_buffer( v_pipeline->shader_binding_table_hit );
            destroy_buffer( v_pipeline->shader_binding_table_miss );
            destroy_buffer( v_pipeline->shader_binding_table_raygen );
        }*/
        // NOTE: pipeline should destroy only resources that are created directly by it,
        // thus this needs to be explicitly handled externally.
        //destroy_shader_state( v_pipeline->shader_state );
    } else {
        ilog_debug( "Graphics error: trying to free invalid Pipeline %u\n", pipeline.index );
    }
}

void GpuDevice::destroy_sampler( SamplerHandle sampler ) {
    if ( sampler.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::Sampler, sampler.index );

        resource_deletion_queue.push( { {sampler.index, sampler.generation}, current_frame, ResourceUpdateType::Sampler } );
    } else {
        ilog_debug( "Graphics error: trying to free invalid Sampler %u\n", sampler.index );
    }
}

void GpuDevice::destroy_descriptor_set_layout( DescriptorSetLayoutHandle descriptor_set_layout ) {
    if ( descriptor_set_layout.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::DescriptorSetLayout, descriptor_set_layout.index );

        resource_deletion_queue.push( { {descriptor_set_layout.index, descriptor_set_layout.generation}, current_frame, ResourceUpdateType::DescriptorSetLayout } );
    } else {
        ilog_debug( "Graphics error: trying to free invalid DescriptorSetLayout %u\n", descriptor_set_layout.index );
    }
}

void GpuDevice::destroy_descriptor_set( DescriptorSetHandle descriptor_set ) {
    if ( descriptor_set.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::DescriptorSet, descriptor_set.index );

        resource_deletion_queue.push( { {descriptor_set.index, descriptor_set.generation}, current_frame, ResourceUpdateType::DescriptorSet } );
    } else {
        ilog_debug( "Graphics error: trying to free invalid DescriptorSet %u\n", descriptor_set.index );
    }
}

void GpuDevice::destroy_shader_state( ShaderStateHandle shader ) {
    if ( shader.is_valid() ) {

        //resource_tracker.track_destroy_resource( ResourceUpdateType::ShaderState, shader.index );

        resource_deletion_queue.push( { {shader.index, shader.generation}, current_frame, ResourceUpdateType::ShaderState } );

        //ShaderState* state = access_shader_state( shader );

        //allocator->deallocate( state->parse_result );
    } else {
        ilog_debug( "Graphics error: trying to free invalid Shader %u\n", shader.index );
    }
}

SwapchainStatus::Enum GpuDevice::update_swapchain() {

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkcheck( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vk_physical_device, vk_window_surface, &surface_capabilities ) );

    const u32 new_width = surface_capabilities.currentExtent.width;
    const u32 new_height = surface_capabilities.currentExtent.height;

    if ( new_width == 0 || new_height == 0 ) {
        return SwapchainStatus::NotReady;
    }

    if ( new_width == swapchain_width && new_height == swapchain_height ) {
        return SwapchainStatus::Ready;
    }

    // Recreate swapchain
    destroy_swapchain();

    create_swapchain();

    vkDeviceWaitIdle( vk_device );

    return SwapchainStatus::Resized;
}

void GpuDevice::create_swapchain() {
    //// Check if surface is supported
    // TODO: Windows only!
    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR( vk_physical_device, queue_indices[ QueueType::Graphics ], vk_window_surface, &surface_supported );
    if ( surface_supported != VK_TRUE ) {
        ilog_error( "Error no WSI support on physical device 0\n" );
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vk_physical_device, vk_window_surface, &surface_capabilities );

    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
    /*if ( swapchain_extent.width == UINT32_MAX ) {
        swapchain_extent.width = clamp( swapchain_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width );
        swapchain_extent.height = clamp( swapchain_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height );
    }*/

    ilog_debug( "Create swapchain %u %u - saved %u %u, min image %u\n", swapchain_extent.width, swapchain_extent.height, swapchain_width, swapchain_height, surface_capabilities.minImageCount );

    swapchain_width = swapchain_extent.width;
    swapchain_height = swapchain_extent.height;

    swapchain_image_count = surface_capabilities.minImageCount < 2 ? 2 : surface_capabilities.minImageCount;

    VkPresentModeKHR vk_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    // Cache old swapchain
    VkSwapchainKHR old_swapchain = vk_swapchain;

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = vk_window_surface,
        .minImageCount = swapchain_image_count,
        .imageFormat = vk_swapchain_format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = vk_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr
    };

    VkResult result = vkCreateSwapchainKHR( vk_device, &swapchain_create_info, vk_allocation_callbacks, &vk_swapchain );
    if ( result != VK_SUCCESS ) {
        ilog_error( "Error creating swapchain\n" );
    }

    // TODO (old): create render pass ?

    //// Cache swapchain images
    vkGetSwapchainImagesKHR( vk_device, vk_swapchain, &swapchain_image_count, NULL );

    BookmarkAllocator* temp_allocator = g_memory->get_thread_allocator();
    sizet marker = temp_allocator->get_marker();
    iassert( swapchain_image_count <= 4 );
    result = vkGetSwapchainImagesKHR( vk_device, vk_swapchain, &swapchain_image_count, vk_swapchain_images );
    if ( result != VK_SUCCESS ) {
        ilog_error( "Error getting swapchain images\n" );
    }

    for ( u32 i = 0; i < swapchain_image_count; ++i ) {

        TextureHandle handle = textures.obtain_object();
        Texture* texture = textures.get_cold( handle );
        VulkanTexture* vk_texture = textures.get_hot( handle );

        *texture = {};
        texture->vk_format = vk_swapchain_format;
        texture->type = TextureType::Texture2D;
        texture->width = swapchain_extent.width;
        texture->height = swapchain_extent.height;

        *vk_texture = {};
        vk_texture->state = ResourceState::Undefined;
        vk_texture->vk_image = vk_swapchain_images[ i ];

        TextureViewCreation tvc = {
           .parent_texture = {0,0}, .view_type = TextureType::Texture2D,
           .sub_resource = {
               .mip_base_level = 0, .mip_level_count = 1,
               .array_base_layer = 0, .array_layer_count = 1
           },
           .debug_name = "swapchain_image_view"
        };

        vulkan_create_texture_view( *this, tvc, texture, vk_texture );

        swapchain_textures[ i ] = handle;
    }

    // Create command buffer ???
    // Command buffer was used only for barriers.
    //
    // for each swapchain image count
    // create texture
    // End and submit command buffer

    temp_allocator->free_marker( marker );
}

void GpuDevice::destroy_swapchain() {

    for ( size_t iv = 0; iv < swapchain_image_count; iv++ ) {

        VulkanTexture* vk_texture = textures.get_hot( swapchain_textures[ iv ] );

        vkDestroyImageView( vk_device, vk_texture->vk_image_view, vk_allocation_callbacks );

        //resource_tracker.track_destroy_resource( ResourceUpdateType::Texture, vk_framebuffer->color_attachments[ a ].index );
        textures.destroy_object( swapchain_textures[ iv ] );

        /*Framebuffer* vk_framebuffer = access_framebuffer( vulkan_swapchain_framebuffers[ iv ] );

        if ( !vk_framebuffer ) {
            continue;
        }

        for ( u32 a = 0; a < vk_framebuffer->num_color_attachments; ++a ) {
            Texture* vk_texture = access_texture( vk_framebuffer->color_attachments[ a ] );

            vkDestroyImageView( vk_device, vk_texture->vk_image_view, vk_allocation_callbacks );

            resource_tracker.track_destroy_resource( ResourceUpdateType::Texture, vk_framebuffer->color_attachments[ a ].index );
            textures.release_resource( vk_framebuffer->color_attachments[ a ].index );
        }

        if ( vk_framebuffer->depth_stencil_attachment.index != k_invalid_index ) {
            resource_tracker.track_destroy_resource( ResourceUpdateType::Texture, vk_framebuffer->depth_stencil_attachment.index );
            destroy_texture_instant( vk_framebuffer->depth_stencil_attachment.index );
        }

        if ( !dynamic_rendering_extension_present ) {
            vkDestroyFramebuffer( vk_device, vk_framebuffer->vk_framebuffer, vk_allocation_callbacks );
        }

        framebuffers.release_resource( vulkan_swapchain_framebuffers[ iv ].index );*/
    }

    vkDestroySwapchainKHR( vk_device, vk_swapchain, vk_allocation_callbacks );
}


void GpuDevice::set_resource_name( VkObjectType type, u64 handle, StringView name ) {

    if ( !debug_utils_extension_present ) {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT name_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    name_info.objectType = type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name.data;
    vkSetDebugUtilsObjectNameEXT( vk_device, &name_info );
}

void* GpuDevice::map_buffer( BufferHandle buffer_handle, u32 offset, u32 size ) {
    if ( buffer_handle.is_invalid() )
        return nullptr;

    Buffer* buffer = buffers.get_cold( buffer_handle );

    void* data;
    vmaMapMemory( vma_allocator, buffer->vma_allocation, &data );

    return data;
}

void GpuDevice::unmap_buffer( BufferHandle buffer_handle ) {
    if ( buffer_handle.is_invalid() )
        return;

    Buffer* buffer = buffers.get_cold( buffer_handle );

    vmaUnmapMemory( vma_allocator, buffer->vma_allocation );
}

void* GpuDevice::dynamic_buffer_allocate( u32 size, u32 alignment, u32* dynamic_offset ) {
    void* mapped_memory = dynamic_mapped_memory + dynamic_allocated_size;
    // Cache the offset to be used.
    *dynamic_offset = dynamic_allocated_size;
    // First find the max alignment between UBO and the struct to be returned.
    u32 max_alignment = ( u32 )idra::mem_align( ubo_alignment, alignment );
    // Then align the allocation based on size.
    dynamic_allocated_size += ( u32 )idra::mem_align( size, max_alignment );
    return mapped_memory;
}

BufferHandle GpuDevice::get_dynamic_buffer() {
    return dynamic_buffer;
}

void GpuDevice::upload_texture_data( TextureHandle texture, void* data ) {
    texture_uploads.push( { texture, data } );
}


void GpuDevice::resize_texture( TextureHandle texture, u32 width, u32 height ) {

    resize_texture_3d( texture, width, height, 1 );
}

void GpuDevice::resize_texture_3d( TextureHandle texture_handle, u32 width, u32 height, u32 depth ) {

    Texture* texture = textures.get_cold( texture_handle );

    if ( texture->width == width && texture->height == height && texture->depth == depth ) {
        return;
    }

    VulkanTexture* vk_texture = textures.get_hot( texture_handle );

    // Queue deletion of texture by creating a temporary one
    TextureHandle texture_to_delete_handle = textures.obtain_object();
    Texture* texture_to_delete = textures.get_cold( texture_to_delete_handle );
    VulkanTexture* vk_texture_to_delete = textures.get_hot( texture_to_delete_handle );

    // Cache all informations (image, image view, flags, ...) into texture to delete.
    // Missing even one information (like it is a texture view, sparse, ...)
    // can lead to memory leaks.
    mem_copy( texture_to_delete, texture, sizeof( Texture ) );
    mem_copy( vk_texture_to_delete, vk_texture, sizeof( VulkanTexture ) );
    // Update handle so it can be used to update bindless to dummy texture
    // and delete the old image and image view.
    texture_to_delete->handle = texture_to_delete_handle;

    // Re-create image in place.
    TextureCreation tc = {
        .width = ( u16 )width, .height = ( u16 )height, .depth = ( u16 )depth,
        .array_layer_count = texture->array_layer_count, .mip_level_count = texture->mip_level_count,
        .flags = texture->flags, .format = texture->format, .type = texture->type,
        .alias = texture->alias_texture, .debug_name = texture->name
    };
    vulkan_create_texture( *this, tc, texture->handle, texture, vk_texture );

    destroy_texture( texture_to_delete_handle );
}

CommandBuffer* GpuDevice::acquire_new_command_buffer() {
    return command_buffer_manager->get_graphics_command_buffer();
}

CommandBuffer* GpuDevice::acquire_command_buffer( u32 index ) {
    return command_buffer_manager->get_graphics_command_buffer();
}

CommandBuffer* GpuDevice::acquire_compute_command_buffer() {
    return command_buffer_manager->get_compute_command_buffer();
}

CommandBuffer* GpuDevice::acquire_transfer_command_buffer() {
    return command_buffer_manager->get_transfer_command_buffer();
}

// Enum translations

VkFormat to_vk_format( TextureFormat::Enum format ) {
    switch ( format ) {

        case TextureFormat::R32G32B32A32_FLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::R32G32B32A32_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case TextureFormat::R32G32B32A32_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case TextureFormat::R32G32B32_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::R32G32B32_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case TextureFormat::R32G32B32_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case TextureFormat::R16G16B16A16_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R16G16B16A16_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case TextureFormat::R16G16B16A16_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case TextureFormat::R16G16B16A16_SNORM:
            return VK_FORMAT_R16G16B16A16_SNORM;
        case TextureFormat::R16G16B16A16_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case TextureFormat::R32G32_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::R32G32_UINT:
            return VK_FORMAT_R32G32_UINT;
        case TextureFormat::R32G32_SINT:
            return VK_FORMAT_R32G32_SINT;
            //case TextureFormat::R10G10B10A2_TYPELESS:
            //    return GL_RGB10_A2;
        case TextureFormat::R10G10B10A2_UNORM:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R10G10B10A2_UINT:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R11G11B10_FLOAT:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            //case TextureFormat::R8G8B8A8_TYPELESS:
            //    return GL_RGBA8;
        case TextureFormat::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
            //case TextureFormat::R8G8B8A8_UNORM_SRGB:
            //    return GL_SRGB8_ALPHA8;
        case TextureFormat::R8G8B8A8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case TextureFormat::R8G8B8A8_SNORM:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case TextureFormat::R8G8B8A8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case TextureFormat::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::B8G8R8X8_UNORM:
            return VK_FORMAT_B8G8R8_UNORM;
            //case TextureFormat::R16G16_TYPELESS:
            //    return GL_RG16UI;
        case TextureFormat::R16G16_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::R16G16_UNORM:
            return VK_FORMAT_R16G16_UNORM;
        case TextureFormat::R16G16_UINT:
            return VK_FORMAT_R16G16_UINT;
        case TextureFormat::R16G16_SNORM:
            return VK_FORMAT_R16G16_SNORM;
        case TextureFormat::R16G16_SINT:
            return VK_FORMAT_R16G16_SINT;
            //case TextureFormat::R32_TYPELESS:
            //    return GL_R32UI;
        case TextureFormat::R32_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::R32_UINT:
            return VK_FORMAT_R32_UINT;
        case TextureFormat::R32_SINT:
            return VK_FORMAT_R32_SINT;
            //case TextureFormat::R8G8_TYPELESS:
            //    return GL_RG8UI;
        case TextureFormat::R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::R8G8_UINT:
            return VK_FORMAT_R8G8_UINT;
        case TextureFormat::R8G8_SNORM:
            return VK_FORMAT_R8G8_SNORM;
        case TextureFormat::R8G8_SINT:
            return VK_FORMAT_R8G8_SINT;
            //case TextureFormat::R16_TYPELESS:
            //    return GL_R16UI;
        case TextureFormat::R16_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::R16_UNORM:
            return VK_FORMAT_R16_UNORM;
        case TextureFormat::R16_UINT:
            return VK_FORMAT_R16_UINT;
        case TextureFormat::R16_SNORM:
            return VK_FORMAT_R16_SNORM;
        case TextureFormat::R16_SINT:
            return VK_FORMAT_R16_SINT;
            //case TextureFormat::R8_TYPELESS:
            //    return GL_R8UI;
        case TextureFormat::R8_UNORM:
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::R8_UINT:
            return VK_FORMAT_R8_UINT;
        case TextureFormat::R8_SNORM:
            return VK_FORMAT_R8_SNORM;
        case TextureFormat::R8_SINT:
            return VK_FORMAT_R8_SINT;
            //// Depth formats
        case TextureFormat::D32_FLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::D32_FLOAT_S8X24_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case TextureFormat::D24_UNORM_X8_UINT:
            return VK_FORMAT_X8_D24_UNORM_PACK32;
        case TextureFormat::D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case TextureFormat::S8_UINT:
            return VK_FORMAT_S8_UINT;

        case TextureFormat::UNKNOWN:
        default:
            //HYDRA_ASSERT( false, "Format %s not supported on Vulkan.", EnumNamesTextureFormat()[format] );
            break;
    }

    return VK_FORMAT_UNDEFINED;
}

//
//
VkImageType to_vk_image_type( TextureType::Enum type ) {
    // Texture1D, Texture2D, Texture3D, TextureCube,
    // Texture_1D_Array, Texture_2D_Array, Texture_Cube_Array, Count
    static VkImageType s_vk_target[ TextureType::Count ] = {
        VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_3D,
        VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D };
    return s_vk_target[ type ];
}

//
//
VkImageViewType to_vk_image_view_type( TextureType::Enum type ) {
    // Texture1D, Texture2D, Texture3D, TextureCube,
    // Texture_1D_Array, Texture_2D_Array, Texture_Cube_Array, Count
    static VkImageViewType s_vk_data[] = {
        VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_VIEW_TYPE_CUBE,
        VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY };
    return s_vk_data[ type ];
}

//
//
VkDescriptorType to_vk_descriptor_type( DescriptorType::Enum type ) {
    // Sampler, Texture, Image, Constants, StructuredBuffer, AccelerationStructure, Count
    static VkDescriptorType s_vk_type[ DescriptorType::Count ] = {
        VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR };
    return s_vk_type[ type ];
}

//
//
VkShaderStageFlagBits to_vk_shader_stage( ShaderStage::Enum value ) {
    // Vertex, Fragment, Compute, RayGen, Intersect, AnyHit, Closest, Miss, Callable, Task, Mesh, Count
    static VkShaderStageFlagBits s_vk_stage[ ShaderStage::Count ] = {
        VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        VK_SHADER_STAGE_MISS_BIT_KHR, VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        VK_SHADER_STAGE_TASK_BIT_EXT, VK_SHADER_STAGE_MESH_BIT_EXT,
    };

    return s_vk_stage[ value ];
}

//
//
static VkFormat to_vk_vertex_format( VertexComponentFormat::Enum value ) {
    // Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Uint, Uint2, Uint4, Count
    static VkFormat s_vk_vertex_formats[ VertexComponentFormat::Count ] = {
        VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, /*MAT4 TODO*/VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R8_SINT, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8_UINT, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32A32_UINT };

    return s_vk_vertex_formats[ value ];
}

//
//
VkCullModeFlags to_vk_cull_mode( CullMode::Enum value ) {
    // TODO: there is also front_and_back!
    static VkCullModeFlags s_vk_cull_mode[ CullMode::Count ] = {
        VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT };
    return s_vk_cull_mode[ value ];
}

//
//
VkFrontFace to_vk_front_face( FrontClockwise::Enum value ) {

    return value == FrontClockwise::True ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

//
//
VkBlendFactor to_vk_blend_factor( Blend::Enum value ) {
    // Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha, DestAlpha, InvDestAlpha, DestColor, InvDestColor, SrcAlphasat, Src1Color, InvSrc1Color, Src1Alpha, InvSrc1Alpha, Count
    static VkBlendFactor s_vk_blend_factor[] = {
        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_SRC1_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA };

    return s_vk_blend_factor[ value ];
}

//
//
VkBlendOp to_vk_blend_operation( BlendOperation::Enum value ) {
    //Add, Subtract, RevSubtract, Min, Max, Count
    static VkBlendOp s_vk_blend_op[] = {
        VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT,
        VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
    return s_vk_blend_op[ value ];
}

//
//
VkCompareOp to_vk_compare_operation( ComparisonFunction::Enum value ) {
    static VkCompareOp s_vk_values[] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS };
    return s_vk_values[ value ];
}

//
//
VkPipelineStageFlags to_vk_pipeline_stage( PipelineStage::Enum value ) {
    static VkPipelineStageFlags s_vk_values[] = { VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
    return s_vk_values[ value ];
}

//
//
// Repeat, Mirrored_Repeat, Clamp_Edge, Clamp_Border, Count
VkSamplerAddressMode to_vk_address_mode( SamplerAddressMode::Enum value ) {
    static VkSamplerAddressMode s_vk_values[] = { VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER };
    return s_vk_values[ value ];
}

//
//
VkFilter to_vk_filter( TextureFilter::Enum value ) {
    static VkFilter s_vk_values[] = { VK_FILTER_NEAREST, VK_FILTER_LINEAR };
    return s_vk_values[ value ];
}

//
//
VkSamplerMipmapMode to_vk_mipmap( SamplerMipmapMode::Enum value ) {
    static VkSamplerMipmapMode s_vk_values[] = {
        VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR };
    return s_vk_values[ value ];
}

//
//
VkIndexType to_vk_index_type( IndexType::Enum type ) {
    static VkIndexType s_vk_values[] = { VK_INDEX_TYPE_UINT16, VK_INDEX_TYPE_UINT32 };
    return s_vk_values[ type ];
}

//
//
VkPolygonMode to_vk_polygon_mode( FillMode::Enum value ) {
    static VkPolygonMode s_vk_polygon_mode[ FillMode::Count ] = { VK_POLYGON_MODE_LINE, VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_POINT };
    return s_vk_polygon_mode[ value ];
}

//
//
VkAccessFlags to_vk_access_flags2( ResourceState::Enum state ) {
    VkAccessFlags ret = 0;
    if ( state & ResourceState::CopySource ) {
        ret |= VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
    }
    if ( state & ResourceState::CopyDest ) {
        ret |= VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    }
    if ( state & ResourceState::VertexAndConstantBuffer ) {
        ret |= VK_ACCESS_2_UNIFORM_READ_BIT_KHR | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
    }
    if ( state & ResourceState::IndexBuffer ) {
        ret |= VK_ACCESS_2_INDEX_READ_BIT_KHR;
    }
    if ( state & ResourceState::UnorderedAccess ) {
        ret |= VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
    }
    if ( state & ResourceState::IndirectArgument ) {
        ret |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;
    }
    if ( state & ResourceState::RenderTarget ) {
        ret |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
    }
    if ( state & ResourceState::DepthWrite ) {
        ret |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
    }
    if ( state & ResourceState::ShaderResource ) {
        ret |= VK_ACCESS_2_SHADER_READ_BIT_KHR;
    }
    if ( state & ResourceState::Present ) {
        ret |= VK_ACCESS_2_MEMORY_READ_BIT_KHR;
    }
    if ( state & ResourceState::ShadingRateSource ) {
        ret |= VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
    }
    if ( state & ResourceState::RaytracingAccelerationStructure ) {
        ret |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    }

    return ret;
}

//
//
VkImageLayout to_vk_image_layout2( ResourceState::Enum usage ) {
    if ( usage & ResourceState::CopySource )
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    if ( usage & ResourceState::CopyDest )
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    if ( usage & ResourceState::RenderTarget )
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

    if ( usage & ResourceState::DepthWrite )
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

    if ( usage & ResourceState::DepthRead )
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

    if ( usage & ResourceState::UnorderedAccess )
        return VK_IMAGE_LAYOUT_GENERAL;

    if ( usage & ResourceState::ShaderResource )
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

    if ( usage & ResourceState::Present )
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    if ( usage == ResourceState::Common )
        return VK_IMAGE_LAYOUT_GENERAL;

    if ( usage == ResourceState::ShadingRateSource )
        return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

//
//
VkPipelineStageFlags2KHR util_determine_pipeline_stage_flags2( VkAccessFlags2KHR access_flags, QueueType::Enum queue_type ) {
    VkPipelineStageFlags2KHR flags = 0;

    switch ( queue_type ) {
        case QueueType::Graphics:
        {
            if ( ( access_flags & ( VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT ) ) != 0 )
                flags |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR;

            if ( ( access_flags & ( VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT ) ) != 0 ) {
                flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
                flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

                // TODO(marco): check RT extension is present/enabled
                //flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            }

            if ( ( access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ) != 0 )
                flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;

            if ( ( access_flags & ( VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR ) ) != 0 )
                flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

            if ( ( access_flags & ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) ) != 0 )
                flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

            if ( ( access_flags & VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR ) != 0 )
                flags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

            if ( ( access_flags & ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) ) != 0 )
                flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;

            break;
        }
        case QueueType::Compute:
        {
            if ( ( access_flags & ( VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT ) ) != 0 ||
                 ( access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ) != 0 ||
                 ( access_flags & ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) ) != 0 ||
                 ( access_flags & ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) ) != 0 )
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

            if ( ( access_flags & ( VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT ) ) != 0 )
                flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

            break;
        }
        case QueueType::Transfer: return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        default: break;
    }

    // Compatible with both compute and graphics queues
    if ( ( access_flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT ) != 0 )
        flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;

    if ( ( access_flags & ( VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT ) ) != 0 )
        flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;

    if ( ( access_flags & ( VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT ) ) != 0 )
        flags |= VK_PIPELINE_STAGE_2_HOST_BIT_KHR;

    if ( flags == 0 )
        flags = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;

    return flags;
}

void GpuDevice::fill_image_barrier( VkImageMemoryBarrier2* barrier, TextureHandle texture_handle, ResourceState::Enum new_state,
                    u32 base_mip_level, u32 mip_count, u32 base_array_layer, u32 array_layer_count,
                    u32 source_family, u32 destination_family,
                    QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type ) {

    Texture* texture = textures.get_cold( texture_handle );
    VulkanTexture* vk_texture = textures.get_hot( texture_handle );
    bool is_depth = TextureFormat::has_depth( texture->format );

    util_fill_image_barrier( barrier, vk_texture->vk_image, vk_texture->state, new_state, base_mip_level,
                        mip_count, base_array_layer, array_layer_count, is_depth,
                        source_family, destination_family, source_queue_type, destination_queue_type );

    // Update texture state
    vk_texture->state = new_state;
}

void GpuDevice::fill_buffer_barrier( VkBufferMemoryBarrier2* barrier, BufferHandle buffer_handle,
                                           ResourceState::Enum new_state, u32 offset, u32 size,
                                           u32 source_family, u32 destination_family,
                                           QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type ) {

    Buffer* buffer = buffers.get_cold( buffer_handle );
    VulkanBuffer* vk_buffer = buffers.get_hot( buffer_handle );

    util_fill_buffer_barrier( barrier, vk_buffer->vk_buffer, buffer->state, new_state,
                              offset, size, source_family, destination_family,
                              source_queue_type, destination_queue_type );
    buffer->state = new_state;
}

void util_fill_image_barrier( VkImageMemoryBarrier2* barrier, VkImage image, ResourceState::Enum old_state, ResourceState::Enum new_state,
                         u32 base_mip_level, u32 mip_count, u32 base_array_layer, u32 array_layer_count,
                         bool is_depth, u32 source_family, u32 destination_family,
                         QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type ) {

    iassert( mip_count > 0 );
    iassert( array_layer_count > 0 );

    // Init to default values
    *barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier->srcAccessMask = to_vk_access_flags2( old_state );
    barrier->srcStageMask = util_determine_pipeline_stage_flags2( barrier->srcAccessMask, source_queue_type );
    barrier->dstAccessMask = to_vk_access_flags2( new_state );
    barrier->dstStageMask = util_determine_pipeline_stage_flags2( barrier->dstAccessMask, destination_queue_type );
    barrier->oldLayout = to_vk_image_layout2( old_state );
    barrier->newLayout = to_vk_image_layout2( new_state );
    barrier->srcQueueFamilyIndex = source_family;
    barrier->dstQueueFamilyIndex = destination_family;
    barrier->image = image;
    barrier->subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier->subresourceRange.baseArrayLayer = base_array_layer;
    barrier->subresourceRange.layerCount = array_layer_count;
    barrier->subresourceRange.baseMipLevel = base_mip_level;
    barrier->subresourceRange.levelCount = mip_count;
}

void util_fill_buffer_barrier( VkBufferMemoryBarrier2KHR* barrier, VkBuffer buffer,
                               ResourceState::Enum old_state, ResourceState::Enum new_state,
                               u32 offset, u32 size, u32 source_family, u32 destination_family,
                               QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type ) {
    // Init to default values
    *barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR };
    barrier->srcAccessMask = to_vk_access_flags2( old_state );
    barrier->srcStageMask = util_determine_pipeline_stage_flags2( barrier->srcAccessMask, source_queue_type );
    barrier->dstAccessMask = to_vk_access_flags2( new_state );
    barrier->dstStageMask = util_determine_pipeline_stage_flags2( barrier->dstAccessMask, destination_queue_type );
    barrier->srcQueueFamilyIndex = source_family;
    barrier->dstQueueFamilyIndex = destination_family;
    barrier->buffer = buffer;
    barrier->offset = offset;
    barrier->size = size == 0 ? VK_WHOLE_SIZE : size;
}

} // namespace idra
