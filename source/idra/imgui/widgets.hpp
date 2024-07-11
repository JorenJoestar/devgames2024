/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "imgui/imgui_helpers.hpp"

#include "kernel/string_view.hpp"
#include "gpu/gpu_resources.hpp"

// Forward declarations
namespace idra {

    struct Camera;
    struct GameCamera;
    struct GpuDevice;
    struct InputSystem;

} // namespace idra

namespace ImGui {


// Application log ////////////////////////////////////////////////

struct ApplicationLog {

    void                init();
    void                shutdown();

    void                clear();
    void                add_log( const char* fmt, ... ) IM_FMTARGS( 2 );

    void                draw( const char* title, bool* p_open = NULL );

    ImGuiTextBuffer     buf;
    ImGuiTextFilter     filter;
    ImVector<int>       line_offsets;        // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a random access on lines
    bool                auto_scroll;         // Keep scrolling if already at the bottom
    bool                open_window = false;

}; // struct ApplicationLog

// Application Log ////////////////////////////////////////////////////////
void                    ApplicationLogInit();
void                    ApplicationLogShutdown();

void                    ApplicationLogDraw();   // Draw log window widget, getting all application logs.


// FPS Graph //////////////////////////////////////////////////////////////
void                    FPSInit( f32 max_value = 33.f );
void                    FPSShutdown();

void                    FPSAdd( f32 delta_time );
void                    FPSDraw( f32 width, f32 height );


// File Dialog ////////////////////////////////////////////////////////////
void                    FileDialogInit();
void                    FileDialogShutdown();

bool                    FileDialogOpen( const char* button_name, const char* path, const char* extension );
const char*             FileDialogGetFilename();

// Directory Dialog ////////////////////////////////////////////////////////
void                    DirectoryDialogInit();
void                    DirectoryDialogShutdown();

bool                    DirectoryDialogOpen( const char* button_name, const char* path );
const char*             DirectoryDialogGetPath();

// Content Browser ////////////////////////////////////////////////////////
void                    ContentBrowserDraw();

// Content Hierarchy //////////////////////////////////////////////////////
void                    ContentHierarchyDraw();

// Render all the IMGUI widgets

//
//
struct ImGuiRenderView {

    static const u32    k_max_textures = 2;

    void                init( idra::GameCamera* camera, idra::Span<const idra::TextureHandle> textures, idra::GpuDevice* gpu );

    void                set_size( ImVec2 size );
    ImVec2              get_size();
    void                check_resize( idra::GpuDevice* gpu, idra::InputSystem* input );

    void                draw( idra::StringView name );

    idra::GameCamera*   camera          = nullptr;

    idra::TextureHandle textures[k_max_textures];
    f32                 texture_width   = 0;
    f32                 texture_height  = 0;

    u32                 num_textures    = 0;
    bool                resized         = false;
    bool                focus           = false;
}; // struct ImGuiRenderView

} // namespace ImGui
