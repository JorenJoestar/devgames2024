/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_resources.hpp"

#include "graphics/render_system_interface.hpp"

namespace idra {

struct ShaderAsset;

// TODO: how to develop this ?
//
//
struct GraphicsPostFullscreenPass {

    void                        render( CommandBuffer* gpu_commands, Camera* camera );

    void                        create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase );
    void                        destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase );

    //
    PipelineHandle              pso;
    DescriptorSetHandle         descriptor_set;

    //
    ShaderAsset*                shader;
    DescriptorSetLayoutHandle   descriptor_set_layout;

}; // struct GraphicsPostFullscreenPass



//
//
struct CRTPostprocess : public RenderSystemInterface {

    //
    //
    struct BlackBoard {

    }; // struct BlackBoard

    //
    //
    struct Externals {

        TextureHandle           input;
        TextureHandle           output;
    }; // struct Externals

    void                        init( GpuDevice* gpu_device, Allocator* resident_allocator ) override;
    void                        shutdown() override;

    void                        update( f32 delta_time );
    void                        render( CommandBuffer* gpu_commands, Camera* camera, u32 phase );

    void                        create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) override;
    void                        destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) override;

    void                        debug_ui();

    GpuDevice*                  gpu_device;

    // Mattias single pass
    ShaderAsset*                mattias_singlepass_shader;
    PipelineHandle              mattias_singlepass_pso;
    DescriptorSetLayoutHandle   mattias_singlepass_dsl;
    DescriptorSetHandle         mattias_singlepass_ds;

    // Newpixie multi pass
    GraphicsPostFullscreenPass  newpixie_accumulation_pass;
    GraphicsPostFullscreenPass  newpixie_blur_pass;
    GraphicsPostFullscreenPass  newpixie_main_pass;

    TextureHandle               newpixie_accumulation_texture;
    TextureHandle               newpixie_previous_horizontal_blur_texture;
    TextureHandle               newpixie_horizontal_blur_texture;
    TextureHandle               newpixie_vertical_blur_texture;

    // Parameters
    enum Types {
        None,
        Lottes,
        Mattias_Singlepass,
        Newpixie_Multipass,
        Count
    }; // enum Types

    i32                         type = None;

    // NEW!
    BlackBoard                  blackboard;
    Externals                   externals;

}; // struct CRTPostprocess

} // namespace idra