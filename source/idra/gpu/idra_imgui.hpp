/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/platform.hpp"
#include "gpu/command_buffer.hpp"

struct ImDrawData;

namespace idra {

    //struct GpuDevice;
    //struct CommandBuffer;
    //using TextureHandle = Handle<struct TextureDummy>;

    //
    //
    enum ImGuiStyles {
        Default = 0,
        GreenBlue,
        DarkRed,
        DarkGold
    }; // enum ImGuiStyles

    //
    //
    struct ImGuiService {

        void                            init( GpuDevice* gpu, void* window_handle );
        void                            shutdown();

        void                            new_frame();
        
        void                            render( CommandBuffer& commands );

        // Removes the Texture from the Cache and destroy the associated Resource List.
        void                            remove_cached_texture( TextureHandle& texture );

        void                            set_style( ImGuiStyles style );

        GpuDevice*                      gpu;

    }; // ImGuiService

    extern ImGuiService*                g_imgui;

} // namespace idra


