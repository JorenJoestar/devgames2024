/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include <stdio.h>

#include "idra_imgui.hpp"

#include "kernel/hash_map.hpp"
#include "kernel/memory.hpp"
#include "kernel/file.hpp"
#include "kernel/string.hpp"

#include "external/imgui/imgui.h"
#include "external/imgui/backends/imgui_impl_sdl2.h"

#include "tools/shader_compiler/shader_compiler.hpp"

#include <vector>

namespace idra {

// NOTE: keeping all data in the source file to avoid including a lot of stuff.
// 
// 
// Graphics Data
idra::TextureHandle         g_font_texture;
idra::ShaderStateHandle     g_shader_state;
idra::PipelineHandle        g_imgui_pipeline;
idra::BufferHandle          g_vb, g_ib;
idra::DescriptorSetLayoutHandle g_descriptor_set_layout;
idra::DescriptorSetHandle   g_ui_descriptor_set;  // Font descriptor set

static u32                  g_vb_size = ikilo( 200 );
static u32                  g_ib_size = ikilo( 200 );
static bool                 s_transition_font_texture = true;

// Upload data
u8*                         vertex_buffer_memory    = nullptr;
u8*                         index_buffer_memory     = nullptr;

idra::FlatHashMap<idra::TextureHandle, idra::DescriptorSetHandle> g_texture_to_descriptor_set;


static const char* g_vertex_shader_code = {
    "#version 450\n"
    "layout( location = 0 ) in vec2 Position;\n"
    "layout( location = 1 ) in vec2 UV;\n"
    "layout( location = 2 ) in uvec4 Color;\n"
    "layout( location = 0 ) out vec2 Frag_UV;\n"
    "layout( location = 1 ) out vec4 Frag_Color;\n"
    "layout( std140, set = 0, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color / 255.0f;\n"
    "    gl_Position = ProjMtx * vec4( Position.xy,0,1 );\n"
    "}\n"
};

static const char* g_vertex_shader_code_bindless = {
    "#version 450\n"
    "layout( location = 0 ) in vec2 Position;\n"
    "layout( location = 1 ) in vec2 UV;\n"
    "layout( location = 2 ) in uvec4 Color;\n"
    "layout( location = 0 ) out vec2 Frag_UV;\n"
    "layout( location = 1 ) out vec4 Frag_Color;\n"
    "layout (location = 2) flat out uint texture_id;\n"
    "layout( std140, set = 1, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color / 255.0f;\n"
    "    texture_id = gl_InstanceIndex;\n"
    "    gl_Position = ProjMtx * vec4( Position.xy,0,1 );\n"
    "}\n"
};

static const char* g_fragment_shader_code = {
    "#version 450\n"
    "layout (location = 0) in vec2 Frag_UV;\n"
    "layout (location = 1) in vec4 Frag_Color;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "layout (set = 0, binding = 1) uniform sampler2D Texture;\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n"
};

static const char* g_fragment_shader_code_bindless = {
    "#version 450\n"
    "#extension GL_EXT_nonuniform_qualifier : enable\n"
    "layout (location = 0) in vec2 Frag_UV;\n"
    "layout (location = 1) in vec4 Frag_Color;\n"
    "layout (location = 2) flat in uint texture_id;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "layout (set = 0, binding = 10) uniform sampler2D textures[];\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(textures[nonuniformEXT(texture_id)], Frag_UV.st);\n"
    "}\n"
};

// Service
static ImGuiService s_imgui_service;
extern ImGuiService* g_imgui = &s_imgui_service;

//
//
void ImGuiService::init( GpuDevice* gpu_, void* window_handle ) {

    gpu = gpu_;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForVulkan( (SDL_Window*)window_handle );

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Idra_ImGui";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Enable Docking

    using namespace idra;

    // Load font texture atlas //////////////////////////////////////////////////
    unsigned char* pixels;
    int width, height;
    // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be
    // compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id,
    // consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    g_font_texture = gpu->create_texture( {.width = (u16)width, .height = (u16)height, .depth = 1, .array_layer_count = 1,
                                          .mip_level_count = 1, .flags = TextureFlags::Default_mask,
                                          .format = TextureFormat::R8G8B8A8_UNORM, .type = TextureType::Texture2D,
                                          .initial_data = pixels, .debug_name = "ImGui_Font" } );


    // Store our identifier
    io.Fonts->TexID = (ImTextureID)&g_font_texture;

    // Manual code. Used to remove dependency from that.
    std::vector<unsigned int> vs_spirv, fs_spirv;
    // Shader compiler DLL interaction.
    if ( gpu->bindless_supported ) {
        shader_compiler_compile( g_vertex_shader_code_bindless, ShaderStage::Vertex, vs_spirv );
        shader_compiler_compile( g_fragment_shader_code_bindless, ShaderStage::Fragment, fs_spirv );
    }
    else {
        shader_compiler_compile( g_vertex_shader_code, ShaderStage::Vertex, vs_spirv );
        shader_compiler_compile( g_fragment_shader_code, ShaderStage::Fragment, fs_spirv );
    }

    g_shader_state = gpu->create_graphics_shader_state( {
        .vertex_shader = {
            .byte_code = Span<u32>( vs_spirv.data(), vs_spirv.size() * 4 ),
            .type = ShaderStage::Vertex } ,
        .fragment_shader = {
            .byte_code = Span<u32>( fs_spirv.data(), fs_spirv.size() * 4 ),
            .type = ShaderStage::Fragment } ,
        .debug_name = "ImGui"} );

    if ( gpu->bindless_supported ) {
        g_descriptor_set_layout = gpu->create_descriptor_set_layout( {
            .dynamic_buffer_bindings = { 0 },
            .debug_name = "imgui_layout" } );

        g_imgui_pipeline = gpu->create_graphics_pipeline( {
            .rasterization = {},
            .depth_stencil = {},
            .blend_state = {.blend_states = { {.source_color = Blend::SrcAlpha,
                                              .destination_color = Blend::InvSrcAlpha,
                                              .color_operation = BlendOperation::Add,
                                               } } },
            .vertex_input = {.vertex_streams = { {.binding = 0, .stride = 20, .input_rate = VertexInputRate::PerVertex} },
                             .vertex_attributes = { { 0, 0, 0, VertexComponentFormat::Float2 },
                                                    { 1, 0, 8, VertexComponentFormat::Float2 },
                                                    { 2, 0, 16, VertexComponentFormat::UByte4N } }},
            .shader = g_shader_state,
            .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, g_descriptor_set_layout },
            .viewport = {},
            .color_formats = { gpu->swapchain_format },
            .debug_name = "Pipeline_ImGui" } );

        g_ui_descriptor_set = gpu->create_descriptor_set( {
            .dynamic_buffer_bindings = {{0,64}},
            .layout = g_descriptor_set_layout,
            .debug_name = "RL_ImGui" } );
    }
    else {
        g_descriptor_set_layout = gpu->create_descriptor_set_layout( {
        .bindings = {
            {.type = DescriptorType::Texture, .start = 1, .count = 1, .name = "Texture"},
        },
        .dynamic_buffer_bindings = { 0 },
        .debug_name = "imgui_layout" } );

        g_imgui_pipeline = gpu->create_graphics_pipeline( {
            .rasterization = {},
            .depth_stencil = {},
            .blend_state = {.blend_states = { {.source_color = Blend::SrcAlpha,
                                              .destination_color = Blend::InvSrcAlpha,
                                              .color_operation = BlendOperation::Add,
                                               } } },
            .vertex_input = {.vertex_streams = { {.binding = 0, .stride = 20, .input_rate = VertexInputRate::PerVertex} },
                             .vertex_attributes = { { 0, 0, 0, VertexComponentFormat::Float2 },
                                                    { 1, 0, 8, VertexComponentFormat::Float2 },
                                                    { 2, 0, 16, VertexComponentFormat::UByte4N } }},
            .shader = g_shader_state,
            .descriptor_set_layouts = { g_descriptor_set_layout },
            .viewport = {},
            .color_formats = { gpu->swapchain_format },
            .debug_name = "Pipeline_ImGui" } );

        g_ui_descriptor_set = gpu->create_descriptor_set( {
            .textures = {{g_font_texture, 1}},
            .dynamic_buffer_bindings = {{0,64}},
            .layout = g_descriptor_set_layout,
            .debug_name = "RL_ImGui" } );
    }

    // Add descriptor set to the map
    // Old Map
    g_texture_to_descriptor_set.init( g_memory->get_resident_allocator(), 4 );
    g_texture_to_descriptor_set.insert( g_font_texture, g_ui_descriptor_set );

    // Create vertex and index buffers //////////////////////////////////////////
    
    g_vb = gpu->create_buffer( {
        .type = BufferUsage::Vertex_mask, .usage = ResourceUsageType::Dynamic,
        .size = g_vb_size * 2, .persistent = 1, .device_only = 0, .initial_data = nullptr,
        .debug_name = "VB_ImGui" } );

    vertex_buffer_memory = gpu->buffers.get_cold( g_vb )->mapped_data;
    
    g_ib = gpu->create_buffer( {
        .type = BufferUsage::Index_mask, .usage = ResourceUsageType::Dynamic,
        .size = g_ib_size * 2, .persistent = 1, .device_only = 0, .initial_data = nullptr,
        .debug_name = "IB_ImGui" } );

    index_buffer_memory = gpu->buffers.get_cold( g_ib )->mapped_data;
}

void ImGuiService::shutdown() {

    FlatHashMapIterator it = g_texture_to_descriptor_set.iterator_begin();
    while ( it.is_valid() ) {
        idra::DescriptorSetHandle handle = g_texture_to_descriptor_set.get( it );
        gpu->destroy_descriptor_set( handle );

        g_texture_to_descriptor_set.iterator_advance( it );
    }

    g_texture_to_descriptor_set.shutdown();

    gpu->destroy_buffer( g_vb );
    gpu->destroy_buffer( g_ib );
    gpu->destroy_descriptor_set_layout( g_descriptor_set_layout );
    gpu->destroy_pipeline( g_imgui_pipeline );
    gpu->destroy_texture( g_font_texture );
    gpu->destroy_shader_state( g_shader_state );

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiService::new_frame() {
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void ImGuiService::render( idra::CommandBuffer& commands ) {

    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)( draw_data->DisplaySize.x * draw_data->FramebufferScale.x );
    int fb_height = (int)( draw_data->DisplaySize.y * draw_data->FramebufferScale.y );
    if ( fb_width <= 0 || fb_height <= 0 )
        return;

    // Vulkan backend has a different origin than OpenGL.
    bool clip_origin_lower_left = false;

#if defined(GL_CLIP_ORIGIN) && !defined(__APPLE__)
    GLenum last_clip_origin = 0; glGetIntegerv( GL_CLIP_ORIGIN, (GLint*)&last_clip_origin ); // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
    if ( last_clip_origin == GL_UPPER_LEFT )
        clip_origin_lower_left = false;
#endif
    size_t vertex_size = draw_data->TotalVtxCount * sizeof( ImDrawVert );
    size_t index_size = draw_data->TotalIdxCount * sizeof( ImDrawIdx );

    if ( vertex_size >= g_vb_size || index_size >= g_ib_size ) {
        ilog_warn( "ImGui Backend Error: vertex/index overflow!\n" );
        return;
    }

    if ( vertex_size == 0 && index_size == 0 ) {
        return;
    }

    using namespace idra;

    // Upload data
    const u32 vertex_memory_offset = commands.gpu_device->current_frame * g_vb_size;
    ImDrawVert* vertex_copy_destination = ( ImDrawVert* )( vertex_buffer_memory + vertex_memory_offset );
    for ( int n = 0; n < draw_data->CmdListsCount; n++ ) {

        const ImDrawList* cmd_list = draw_data->CmdLists[ n ];
        memcpy( vertex_copy_destination, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
        vertex_copy_destination += cmd_list->VtxBuffer.Size;
    }

    const u32 index_memory_offset = commands.gpu_device->current_frame * g_ib_size;
    ImDrawIdx* index_copy_destination = ( ImDrawIdx* )( index_buffer_memory + index_memory_offset );

    for ( int n = 0; n < draw_data->CmdListsCount; n++ ) {

        const ImDrawList* cmd_list = draw_data->CmdLists[ n ];
        memcpy( index_copy_destination, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
        index_copy_destination += cmd_list->IdxBuffer.Size;
    }

    // Do not bind any specific pass - this should be done externally.
    commands.push_marker( "ImGUI" );

    commands.bind_pipeline( g_imgui_pipeline );
    commands.bind_vertex_buffer( g_vb, 0, vertex_memory_offset );
    commands.bind_index_buffer( g_ib, index_memory_offset, IndexType::Uint16 );

    commands.set_viewport( { 0, 0, ( u16 )fb_width, ( u16 )fb_height, 0.0f, 1.0f } );

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
    };

    u32 cb_offset;
    float* cb_data = (float*)gpu->dynamic_buffer_allocate( 64, alignof(float), &cb_offset );
    if ( cb_data ) {
        memcpy( cb_data, &ortho_projection[0][0], 64 );
    }

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    //
    int counts = draw_data->CmdListsCount;
    
    TextureHandle last_texture = g_font_texture;
    // todo:map
    DescriptorSetHandle last_descriptor_set = { g_texture_to_descriptor_set.get( last_texture ) };

    if ( gpu->bindless_supported ) {
        commands.bind_descriptor_set( { gpu->bindless_descriptor_set, last_descriptor_set }, { cb_offset } );
    }
    else {
        commands.bind_descriptor_set( { last_descriptor_set }, { cb_offset } );
    }

    uint32_t vtx_buffer_offset = 0, index_buffer_offset = 0;
    for ( int n = 0; n < counts; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        for ( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if ( pcmd->UserCallback )
            {
                // User callback (registered via ImDrawList::AddCallback)
                pcmd->UserCallback( cmd_list, pcmd );
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = ( pcmd->ClipRect.x - clip_off.x ) * clip_scale.x;
                clip_rect.y = ( pcmd->ClipRect.y - clip_off.y ) * clip_scale.y;
                clip_rect.z = ( pcmd->ClipRect.z - clip_off.x ) * clip_scale.x;
                clip_rect.w = ( pcmd->ClipRect.w - clip_off.y ) * clip_scale.y;

                if ( clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f )
                {
                    // Apply scissor/clipping rectangle
                    if ( clip_origin_lower_left ) {
                        Rect2DInt scissor_rect = { (int16_t)clip_rect.x, (int16_t)( fb_height - clip_rect.w ), (uint16_t)( clip_rect.z - clip_rect.x ), (uint16_t)( clip_rect.w - clip_rect.y ) };
                        commands.set_scissor( scissor_rect );
                    }
                    else {
                        Rect2DInt scissor_rect = { (int16_t)clip_rect.x, (int16_t)clip_rect.y, (uint16_t)( clip_rect.z - clip_rect.x ), (uint16_t)( clip_rect.w - clip_rect.y ) };
                        commands.set_scissor( scissor_rect );
                    }

                    // Retrieve
                    TextureHandle new_texture = *(TextureHandle*)( pcmd->TextureId );
                    if ( !gpu->bindless_supported ) {
                        if ( new_texture.index != last_texture.index && new_texture.is_valid() ) {
                            last_texture = new_texture;
                            FlatHashMapIterator it = g_texture_to_descriptor_set.find( last_texture );

                            // TODO: invalidate handles and update descriptor set when needed ?
                            // Found this problem when reusing the handle from a previous
                            // If not present
                            if ( it.is_invalid() ) {
                                // Create new descriptor set
                                last_descriptor_set = gpu->create_descriptor_set( { 
                                    .textures = { {last_texture, 1} },
                                    .buffers = {},
                                    .dynamic_buffer_bindings = {{0,64}},
                                    .layout = g_descriptor_set_layout,
                                    .debug_name = "RL_Dynamic_ImGUI" } );

                                g_texture_to_descriptor_set.insert( new_texture, last_descriptor_set );
                            } else {
                                last_descriptor_set = g_texture_to_descriptor_set.get( it );
                            }
                            commands.bind_descriptor_set( { last_descriptor_set }, {} );
                        }
                    }

                    commands.draw_indexed( idra::TopologyType::Triangle, pcmd->ElemCount, 1, index_buffer_offset + pcmd->IdxOffset, vtx_buffer_offset + pcmd->VtxOffset, new_texture.index );
                }
            }

        }
        index_buffer_offset += cmd_list->IdxBuffer.Size;
        vtx_buffer_offset += cmd_list->VtxBuffer.Size;
    }

    commands.pop_marker();
}

static void set_style_dark_gold();
static void set_style_green_blue();
static void set_style_dark_red();

void ImGuiService::set_style( ImGuiStyles style ) {

    switch ( style ) {
        case GreenBlue:
        {
            set_style_green_blue();
            break;
        }

        case DarkRed:
        {
            set_style_dark_red();
            break;
        }

        case DarkGold:
        {
            set_style_dark_gold();
            break;
        }

        default:
        case Default:
        {
            ImGui::StyleColorsDark();
            break;
        }
    }
}

void ImGuiService::remove_cached_texture( idra::TextureHandle& texture ) {
    FlatHashMapIterator it = g_texture_to_descriptor_set.find( texture );
    if ( it.is_valid() ) {

        // Destroy descriptor set
        idra::DescriptorSetHandle descriptor_set{ g_texture_to_descriptor_set.get(it) };
        gpu->destroy_descriptor_set( descriptor_set );

        // Remove from cache
        g_texture_to_descriptor_set.remove( texture );
    }
}

void set_style_dark_red() {
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ ImGuiCol_Text ] = ImVec4( 0.75f, 0.75f, 0.75f, 1.00f );
    colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.35f, 0.35f, 0.35f, 1.00f );
    colors[ ImGuiCol_WindowBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.94f );
    colors[ ImGuiCol_ChildBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_PopupBg ] = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
    colors[ ImGuiCol_Border ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.50f );
    colors[ ImGuiCol_BorderShadow ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_FrameBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.54f );
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.37f, 0.14f, 0.14f, 0.67f );
    colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.39f, 0.20f, 0.20f, 0.67f );
    colors[ ImGuiCol_TitleBg ] = ImVec4( 0.04f, 0.04f, 0.04f, 1.00f );
    colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.48f, 0.16f, 0.16f, 1.00f );
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4( 0.48f, 0.16f, 0.16f, 1.00f );
    colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
    colors[ ImGuiCol_CheckMark ] = ImVec4( 0.56f, 0.10f, 0.10f, 1.00f );
    colors[ ImGuiCol_SliderGrab ] = ImVec4( 1.00f, 0.19f, 0.19f, 0.40f );
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.89f, 0.00f, 0.19f, 1.00f );
    colors[ ImGuiCol_Button ] = ImVec4( 1.00f, 0.19f, 0.19f, 0.40f );
    colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.80f, 0.17f, 0.00f, 1.00f );
    colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.89f, 0.00f, 0.19f, 1.00f );
    colors[ ImGuiCol_Header ] = ImVec4( 0.33f, 0.35f, 0.36f, 0.53f );
    colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.76f, 0.28f, 0.44f, 0.67f );
    colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.47f, 0.47f, 0.47f, 0.67f );
    colors[ ImGuiCol_Separator ] = ImVec4( 0.32f, 0.32f, 0.32f, 1.00f );
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4( 0.32f, 0.32f, 0.32f, 1.00f );
    colors[ ImGuiCol_SeparatorActive ] = ImVec4( 0.32f, 0.32f, 0.32f, 1.00f );
    colors[ ImGuiCol_ResizeGrip ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.85f );
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.60f );
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.90f );
    colors[ ImGuiCol_Tab ] = ImVec4( 0.07f, 0.07f, 0.07f, 0.51f );
    colors[ ImGuiCol_TabHovered ] = ImVec4( 0.86f, 0.23f, 0.43f, 0.67f );
    colors[ ImGuiCol_TabActive ] = ImVec4( 0.19f, 0.19f, 0.19f, 0.57f );
    colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.05f, 0.05f, 0.05f, 0.90f );
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.13f, 0.13f, 0.13f, 0.74f );
#if defined(IMGUI_HAS_DOCK)
    colors[ ImGuiCol_DockingPreview ] = ImVec4( 0.47f, 0.47f, 0.47f, 0.47f );
    colors[ ImGuiCol_DockingEmptyBg ] = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
#endif // IMGUI_HAS_DOCK
    colors[ ImGuiCol_PlotLines ] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ ImGuiCol_PlotLinesHovered ] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ ImGuiCol_PlotHistogram ] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
#if defined(IMGUI_HAS_TABLE)
    colors[ ImGuiCol_TableHeaderBg ] = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
    colors[ ImGuiCol_TableBorderStrong ] = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
    colors[ ImGuiCol_TableBorderLight ] = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
    colors[ ImGuiCol_TableRowBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_TableRowBgAlt ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.07f );
#endif // IMGUI_HAS_TABLE
    colors[ ImGuiCol_TextSelectedBg ] = ImVec4( 0.26f, 0.59f, 0.98f, 0.35f );
    colors[ ImGuiCol_DragDropTarget ] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.26f, 0.59f, 0.98f, 1.00f );
    colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ ImGuiCol_ModalWindowDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
}


void set_style_green_blue() {
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ ImGuiCol_Text ] = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );
    colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ ImGuiCol_WindowBg ] = ImVec4( 0.06f, 0.06f, 0.06f, 0.94f );
    colors[ ImGuiCol_ChildBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_PopupBg ] = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
    colors[ ImGuiCol_Border ] = ImVec4( 0.43f, 0.43f, 0.50f, 0.50f );
    colors[ ImGuiCol_BorderShadow ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_FrameBg ] = ImVec4( 0.44f, 0.44f, 0.44f, 0.60f );
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.57f, 0.57f, 0.57f, 0.70f );
    colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.76f, 0.76f, 0.76f, 0.80f );
    colors[ ImGuiCol_TitleBg ] = ImVec4( 0.04f, 0.04f, 0.04f, 1.00f );
    colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.16f, 0.16f, 0.16f, 1.00f );
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.60f );
    colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
    colors[ ImGuiCol_CheckMark ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.80f );
    colors[ ImGuiCol_SliderGrab ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.80f );
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Button ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Header ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Separator ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_SeparatorActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_ResizeGrip ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Tab ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.80f );
    colors[ ImGuiCol_TabHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.80f );
    colors[ ImGuiCol_TabActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.18f, 0.18f, 0.18f, 1.00f );
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.36f, 0.36f, 0.36f, 0.54f );
#if defined(IMGUI_HAS_DOCK)
    colors[ ImGuiCol_DockingPreview ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.80f );
    colors[ ImGuiCol_DockingEmptyBg ] = ImVec4( 0.13f, 0.13f, 0.13f, 0.80f );
#endif // IMGUI_HAS_DOCK
    colors[ ImGuiCol_PlotLines ] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ ImGuiCol_PlotLinesHovered ] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ ImGuiCol_PlotHistogram ] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
#if defined (IMGUI_HAS_TABLE)
    colors[ ImGuiCol_TableHeaderBg ] = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
    colors[ ImGuiCol_TableBorderStrong ] = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
    colors[ ImGuiCol_TableBorderLight ] = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
    colors[ ImGuiCol_TableRowBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_TableRowBgAlt ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.07f );
#endif // IMGUI_HAS_TABLE
    colors[ ImGuiCol_TextSelectedBg ] = ImVec4( 0.26f, 0.59f, 0.98f, 0.35f );
    colors[ ImGuiCol_DragDropTarget ] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.26f, 0.59f, 0.98f, 1.00f );
    colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ ImGuiCol_ModalWindowDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
}

static void set_style_dark_gold() {
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ ImGuiCol_Text ] = ImVec4( 0.92f, 0.92f, 0.92f, 1.00f );
    colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.44f, 0.44f, 0.44f, 1.00f );
    colors[ ImGuiCol_WindowBg ] = ImVec4( 0.06f, 0.06f, 0.06f, 1.00f );
    colors[ ImGuiCol_ChildBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_PopupBg ] = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
    colors[ ImGuiCol_Border ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_BorderShadow ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_FrameBg ] = ImVec4( 0.11f, 0.11f, 0.11f, 1.00f );
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_TitleBg ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
    colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.11f, 0.11f, 0.11f, 1.00f );
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.06f, 0.06f, 0.06f, 0.53f );
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.21f, 0.21f, 0.21f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.47f, 0.47f, 0.47f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.81f, 0.83f, 0.81f, 1.00f );
    colors[ ImGuiCol_CheckMark ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_SliderGrab ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_Button ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_Header ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.93f, 0.65f, 0.14f, 1.00f );
    colors[ ImGuiCol_Separator ] = ImVec4( 0.21f, 0.21f, 0.21f, 1.00f );
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_SeparatorActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_ResizeGrip ] = ImVec4( 0.21f, 0.21f, 0.21f, 1.00f );
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_Tab ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_TabHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_TabActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.07f, 0.10f, 0.15f, 0.97f );
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.14f, 0.26f, 0.42f, 1.00f );
    colors[ ImGuiCol_PlotLines ] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ ImGuiCol_PlotLinesHovered ] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ ImGuiCol_PlotHistogram ] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
    colors[ ImGuiCol_TextSelectedBg ] = ImVec4( 0.26f, 0.59f, 0.98f, 0.35f );
    colors[ ImGuiCol_DragDropTarget ] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.26f, 0.59f, 0.98f, 1.00f );
    colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ ImGuiCol_ModalWindowDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );

    style->FramePadding = ImVec2( 4, 2 );
    style->ItemSpacing = ImVec2( 10, 2 );
    style->IndentSpacing = 12;
    style->ScrollbarSize = 10;

    style->WindowRounding = 4;
    style->FrameRounding = 4;
    style->PopupRounding = 4;
    style->ScrollbarRounding = 6;
    style->GrabRounding = 4;
    style->TabRounding = 4;

    style->WindowTitleAlign = ImVec2( 1.0f, 0.5f );
    style->WindowMenuButtonPosition = ImGuiDir_Right;

    style->DisplaySafeAreaPadding = ImVec2( 4, 4 );
}

} // namespace idra
