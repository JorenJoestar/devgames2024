/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string_view.hpp"

namespace idra {

    ///
    /// @brief Struct used to configure the application.
    ///
    struct ApplicationConfiguration {

        u32                         width       = 1;
        u32                         height      = 1;

        StringView                  name;

    }; // struct ApplicationConfiguration

    ///
    /// @brief Application interface.
    /// 
    struct Application {

        // 
        virtual void                create( const ApplicationConfiguration& configuration ) {}
        virtual void                destroy() {}
        virtual bool                main_loop() { return false; }

        // Fixed update. Can be called more than once compared to rendering.
        virtual void                fixed_update( f32 delta ) {}
        // Variable time update. Called only once per frame.
        virtual void                variable_update( f32 delta ) {}
        // Rendering with optional interpolation factor.
        virtual void                render( f32 interpolation ) {}

        // Load/unload resources callback. Type is used as a user-defined way
        // to separate which resources are loaded/unloaded.
        virtual void                load_resource( u32 type ) {}
        virtual void                unload_resource( u32 type ) {}

        // Per frame begin/end.
        virtual void                frame_begin() {}
        virtual void                frame_end() {}

        void                        run( const ApplicationConfiguration& configuration );

    }; // struct Application

} // namespace idra