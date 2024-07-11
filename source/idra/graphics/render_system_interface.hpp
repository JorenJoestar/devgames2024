/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/platform.hpp"

namespace idra {

struct Allocator;
struct AssetManager;
struct Camera;
struct CommandBuffer;
struct GpuDevice;

namespace AssetCreationPhase {
enum Enum : u8;
} // namespace AssetCreationPhase

namespace AssetDestructionPhase {
enum Enum : u8;
} // namespace AssetDestructionPhase

struct RenderSystemInterface {

    virtual void            init( GpuDevice* gpu, Allocator* allocator ) = 0;
    virtual void            shutdown() = 0;

    virtual void            create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) = 0;
    virtual void            destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) = 0;

    // TODO: update context
    virtual void            update( f32 delta_time ) = 0;
    // TODO: render context
    virtual void            render( CommandBuffer* cb, Camera* camera, u32 phase ) = 0;

}; // struct RenderSystemInterface

} // namespace idra
