
layout (location = 0) in vec2 point_a;
layout (location = 1) in uvec4 color_a;
layout (location = 2) in vec2 point_b;
layout (location = 3) in uvec4 color_b;

layout (location = 0) out vec4 Frag_Color;
layout (location = 1) out vec2 Frag_UV;

vec2 segmentInstanceGeometry[6] = { vec2(0, -0.5), vec2(1, -0.5), vec2(1, 0.5), vec2(0, -0.5), vec2(1, 0.5), vec2(0, 0.5)};
vec2 uv[6] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1)};

void main()
{
    // 2D Line working
    vec2 position = segmentInstanceGeometry[gl_VertexIndex % 6];
    vec2 x_basis = point_b.xy - point_a.xy;
    vec2 y_basis = normalize( vec2(-x_basis.y, x_basis.x) );

    const float width = 0.005;
    vec3 point = vec3(point_a.xy + x_basis * position.x + y_basis * width * position.y, 0);
    gl_Position = vec4(point.xyz, 1.0f); // * ortho projection ?

    // Colors are stored in a Uint decompressed to 4 floats, but they range from 0 to 255.
    Frag_Color = mix(color_a, color_b, position.x) / 255.f;
    Frag_UV = uv[gl_VertexIndex % 6];
}