
layout ( std140, set = 0, binding = 0 ) uniform SceneConstants {
    mat4        view_projection;

    vec2        resolution;
    float       sc000;
    float       sc001;
};


layout (location = 0) in vec3 point_a;
layout (location = 1) in uvec4 color_a;
layout (location = 2) in vec3 point_b;
layout (location = 3) in uvec4 color_b;

layout (location = 0) out vec4 Frag_Color;
layout (location = 1) out vec2 Frag_UV;

// X and Y guide the expansion in clip space, Z guides the part of the segment the final vertex will be pointing to.
vec3 segment_quad[6] = { vec3(-0.5, -0.5, 0), vec3(0.5, -0.5, 1), vec3(0.5, 0.5, 1), vec3(-0.5, -0.5, 0), vec3(0.5, 0.5, 1), vec3(-0.5, 0.5, 0)};
vec2 uv[6] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1)};

float expansion_direction[6] = { 1, -1, -1, 1, -1, 1 };

void main()
{
    // Based on "Antialised volumetric lines using shader based extrusion" by Sebastien Hillaire, OpenGL Insights Chapter 11.
    vec3 position = segment_quad[gl_VertexIndex % 6];
    const float width = 0.005;

    vec4 clip0 = view_projection * vec4(point_a, 1.0);
    vec4 clip1 = view_projection * vec4(point_b, 1.0);

    vec2 line_direction = width * normalize( (clip1.xy / clip1.w) - (clip0.xy / clip0.w) );
    if ( clip1.w * clip0.w  < 0 ) {
        line_direction = -line_direction;
    }


    float segment_length = length(clip1.xy + clip0.xy);
    vec2 aspect_ratio = vec2( 1.0, resolution.x / resolution.y );
    gl_Position = mix(clip0, clip1, position.z);
    
    // Vary width based on clip space Z - becomes more stable in screen space.
    line_direction *= gl_Position.z;
    
    gl_Position.xy += line_direction.xy * position.xx * aspect_ratio;
    gl_Position.xy += line_direction.yx * position.yy * vec2(1, -1) * aspect_ratio;

    Frag_Color = mix(color_a, color_b, position.z) / 255.f;
    Frag_UV = uv[gl_VertexIndex % 6];
}

