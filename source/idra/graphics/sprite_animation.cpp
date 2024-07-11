#include "graphics/sprite_animation.hpp"

#include "kernel/numerics.hpp"

#include "cglm/struct/vec2.h"
#include "cglm/util.h"

namespace idra {

void SpriteAnimationSystem::init( Allocator* allocator_, u32 size ) {
    allocator = allocator_;

    data.init( allocator, size );
    states.init( allocator, size );
}

void SpriteAnimationSystem::shutdown() {
    data.shutdown();
    states.shutdown();
}

static void set_time( SpriteAnimationState* state, const SpriteAnimationData& data, f32 time ) {
    state->current_time = time;

    const u32 num_frames = data.frame_table.size ? (u32)data.frame_table.size : data.num_frames;
    const f32 duration = f32( num_frames ) / data.fps;
    u32 frame = flooru32(num_frames * ( time / duration ));

    if ( time > duration ) {
        if ( data.is_inverted ) {
            state->inverted = data.is_inverted ? !state->inverted : false;
            // Remove/add a frame depending on the direction
            const f32 frame_length = 1.0f / data.fps;
            state->current_time -= duration - frame_length;
        }
        else {
            state->current_time -= duration;
        }
    }

    if ( data.is_looping )
        frame %= num_frames;
    else
        frame = min( frame, (u32)num_frames - 1);

    frame = state->inverted ? num_frames - 1 - frame : frame;

    //hprint( "Frame %u, %f %f\n", frame, time, duration );

    const u32 sprite_frame = data.frame_table.size ? data.frame_table[ frame ] : frame;
    u32 frame_x = sprite_frame % data.frames_columns;
    u32 frame_y = sprite_frame / data.frames_columns;

    // Horizontal only scroll. Change U0 and U1 only.
    state->uv_offset = glms_vec2_add( data.uv_offset, vec2s{ data.uv_size.x * frame_x, data.uv_size.y * frame_y } );
    state->uv_size = data.uv_size;
}

void SpriteAnimationSystem::start_animation( SpriteAnimationState* animation, SpriteAnimationHandle handle, bool restart ) {
    if ( handle != animation->handle || restart ) {
        const SpriteAnimationData& animation_data = *data.get( handle );
        set_time( animation, animation_data, 0.f);
        animation->handle = handle;
        animation->inverted = false;
        // Copy single frame size
        animation->width = animation_data.frame_width;
        animation->height = animation_data.frame_height;
    }
}

void SpriteAnimationSystem::update_animation( SpriteAnimationState* animation, f32 delta_time ) {
    set_time( animation, *data.get( animation->handle ), animation->current_time + delta_time);
}

f32 SpriteAnimationSystem::get_duration( const SpriteAnimationState* animation ) const {
    const SpriteAnimationData& animation_data = *data.get( animation->handle );
    return f32( animation_data.num_frames ) / animation_data.fps;
}

bool SpriteAnimationSystem::is_finished( const SpriteAnimationState* animation ) const {
    const SpriteAnimationData& animation_data = *data.get( animation->handle );
    const f32 duration = f32( animation_data.num_frames ) / animation_data.fps;
    return animation_data.is_looping ? false : animation->current_time >= duration;
}

SpriteAnimationHandle SpriteAnimationSystem::create_animation( const SpriteAnimationCreation& creation ) {
    SpriteAnimationData& new_data = *data.obtain();

    const f32 rcp_texture_width = 1.f / ( creation.texture_width - 1 );
    const f32 rcp_texture_height = 1.f / ( creation.texture_height - 1 );

    new_data.frame_width = creation.frame_width;
    new_data.frame_height = creation.frame_height;
    new_data.uv_offset = { creation.offset_x * rcp_texture_width, creation.offset_y * rcp_texture_height };
    new_data.uv_size = { creation.frame_width * rcp_texture_width, creation.frame_height * rcp_texture_height };
    new_data.num_frames = creation.num_frames;
    new_data.frames_columns = creation.columns;
    new_data.fps = creation.fps;
    new_data.is_looping = creation.looping;
    new_data.is_inverted = creation.invert;
    new_data.frame_table = creation.frame_table_;

    return new_data.pool_index;
}

void SpriteAnimationSystem::destroy_animation( SpriteAnimationHandle handle ) {

    data.release_resource( handle );
}

SpriteAnimationState* SpriteAnimationSystem::create_animation_state() {
    return states.obtain();
}

void SpriteAnimationSystem::destroy_animation_state( SpriteAnimationState* state ) {
    states.release( state );
}


// Utils ////////////////////////////////////////////////////////////////////////
Direction8::Enum idra::Direction8::from_axis( f32 x, f32 y ) {
    const f32 angle = atan2f( y, x );
    const u32 octant = roundu32( 8 * angle / ( GLM_PI * 2.f ) + 8 ) % 8;
    return Direction8::Enum(octant);
}

Direction4::Enum idra::Direction4::from_axis( f32 x, f32 y ) {
    const f32 angle = atan2f( y, x );
    const u32 quadrant = roundu32( 4 * angle / (GLM_PI * 2.f) + 4 ) % 4;
    return Direction4::Enum( quadrant );
}


} // namespace idra
