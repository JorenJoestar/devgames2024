
struct LocalConstants {
    mat4          	view_projection_matrix;
    mat4            projection_matrix_2d;

    uint            screen_width;
    uint            screen_height;
    uint            padding0;
    uint            padding1;
};

layout (std140, set=1, binding=0)
uniform Local{
	LocalConstants  locals;
};


//#define FLIP_Y
//#pragma include "debug_gpu_font.h"


layout (location = 0) out vec2 uv;
layout (location = 1) flat out uint global_data_index;

// Per vertex positions and uvs of a quad
vec3 positions[6]       = vec3[6]( vec3(-0.5,-0.5,0), vec3(0.5,-0.5,0), vec3(0.5, 0.5, 0), vec3(0.5, 0.5, 0), vec3(-0.5,0.5,0), vec3(-0.5,-0.5,0) );
vec2 uvs[6]             = vec2[6]( vec2(0.0, 1.0),    vec2(1.0, 1.0),   vec2(1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 1.0) );

void main() {

    const uint vertex_index = gl_VertexIndex % 6;
    // Calculate UVs
    uv.xy = uvs[vertex_index];
    uv.y = 1 - uv.y;

    // Sprite size
    vec2 sprite_size = CHAR_SIZE;
    //sprite_size.x /= locals.screen_width * 1.0f / locals.screen_height;
    //sprite_size.y /= locals.screen_width * 1.0f / locals.screen_height;
    // Calculate world position
    vec4 world_position = vec4( vec2(positions[ vertex_index ].xy * sprite_size ), 0, 1 );

    uint global_char_index = gl_InstanceIndex;
    uint entry_index = dispatches[global_char_index].x;
    uint entry_char_index = dispatches[global_char_index].y;

    DebugGPUStringEntry entry = entries[entry_index];
    world_position.xy += vec2(entry.x + entry_char_index * sprite_size.x, entry.y);
    // Move position to upper left corner
    world_position.xy += sprite_size * 0.5f;
    //world_position.xy = (world_position.xy / vec2(locals.screen_width * .5, locals.screen_height * .5)) - vec2(1,1);

    global_data_index = entry.offset + entry_char_index;

    gl_Position = locals.projection_matrix_2d * world_position;

    //gl_Position = world_position;
}
