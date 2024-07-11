/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/memory.hpp"
#include "kernel/string_view.hpp"

namespace idra {

    struct InputSystem;

    ///
    /// @brief Struct that handles an OS window.
    /// 
    struct Window {

        void            init( u32 width, u32 height, StringView name, Allocator* allocator, InputSystem* input );
        void            shutdown();

        void            handle_os_messages();

        void            set_fullscreen( bool value );
        void            center_mouse( bool dragging );

        void*           platform_handle     = nullptr;
        bool            is_running          = false;
        bool            resized             = false;
        bool            minimized           = false;
        u32             width               = 0;
        u32             height              = 0;
        f32             display_refresh     = 1.0f / 60.0f;

        InputSystem*    input;

    }; // struct Window

} // namespace idra