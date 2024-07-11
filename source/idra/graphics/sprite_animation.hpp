#pragma once


#include "kernel/platform.hpp"
#include "kernel/pool.hpp"
#include "kernel/string_view.hpp"

#include "cglm/types-struct.h"

namespace idra {

typedef u32     SpriteAnimationHandle;

//
//
struct SpriteAnimationCreation {

    Span<const u16>         frame_table_{};

    u16                     texture_width;
    u16                     texture_height;
    u16                     offset_x;
    u16                     offset_y;
    u16                     frame_width;
    u16                     frame_height;
    u16                     num_frames;
    u16                     columns;

    u8                      fps;

    bool                    looping;
    bool                    invert;

}; // struct AnimationCreation

//
//
struct SpriteAnimationData {

    vec2s       uv_offset;
    vec2s       uv_size;

    u32         pool_index;

    // Total number of frames
    u16         num_frames;
    // Columns for grid animations.
    u16         frames_columns;
    u16         frame_width;
    u16         frame_height;

    u8          fps;
    bool        is_looping;
    bool        is_inverted;    // Invert animation for ping-pong between frames

    Span<const u16>   frame_table;

    StringView  name;

}; // struct AnimationData

//
//
struct SpriteAnimationState {
    SpriteAnimationHandle handle;
    f32         current_time;

    vec2s       uv_offset;
    vec2s       uv_size;

    u32         pool_index;
    u16         width;
    u16         height;

    StringView  name;

    bool        inverted;

}; // struct AnimationState


//
//
struct SpriteAnimationSystem {

    void                    init( Allocator* allocator, u32 size );
    void                    shutdown();

    // Start animation only if it is new or explicitly restarting
    void                    start_animation( SpriteAnimationState* animation, SpriteAnimationHandle handle, bool restart );
    void                    update_animation( SpriteAnimationState* animation, f32 delta_time );

    f32                     get_duration( const SpriteAnimationState* animation ) const;
    bool                    is_finished( const SpriteAnimationState* animation ) const;

    SpriteAnimationHandle   create_animation( const SpriteAnimationCreation& creation );
    void                    destroy_animation( SpriteAnimationHandle handle );

    SpriteAnimationState*   create_animation_state();
    void                    destroy_animation_state( SpriteAnimationState* state );

    ResourcePoolTyped<SpriteAnimationData>  data;
    ResourcePoolTyped<SpriteAnimationState> states;
    Allocator*              allocator;

}; // struct SpriteAnimationSystem


// Utils ////////////////////////////////////////////////////////////////////////
namespace Direction8 {
    enum Enum {
        Right, TopRight, Top, TopLeft, Left, BottomLeft, Bottom, BottomRight, Count
    };

    Direction8::Enum        from_axis( f32 x, f32 y );
}

namespace Direction4 {
    enum Enum {
        Right, Top, Left, Bottom, Count
    };

    Direction4::Enum        from_axis( f32 x, f32 y );
}

} // namespace idra


