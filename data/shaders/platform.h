
#if defined (FULLSCREEN_TRI) && defined(VERTEX)

layout (location = 0) out vec2 vTexCoord;
layout (location = 1) flat out uint out_texture_id;

void main() {

    vTexCoord.xy = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(vTexCoord.xy * 2.0f - 1.0f, 0.0f, 1.0f);
    gl_Position.y = -gl_Position.y;

    out_texture_id = gl_InstanceIndex;
}

#endif // FULLSCREEN_TRI

vec3 world_position_from_depth( vec2 uv, float raw_depth, mat4 inverse_view_projection ) {

    vec4 H = vec4( uv.x * 2 - 1, uv.y * -2 + 1, raw_depth * 2 - 1, 1 );
    vec4 D = inverse_view_projection * H;

    return D.xyz / D.w;
}

#define IDRA_BINDLESS

#if defined(IDRA_BINDLESS)

#define GLOBAL_SET 0
#define MATERIAL_SET 1
#define SHADER_SET 2
#define DYNAMIC_SET 3

#define BINDLESS_BINDING 10
#define BINDLESS_IMAGES 11

//#extension GL_ARB_shader_draw_parameters : enable

//Bindless support //////////////////////////////////////////////////////
//Enable non uniform qualifier extension
#extension GL_EXT_nonuniform_qualifier : enable

layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform sampler2D global_textures[];
// Alias textures to use the same binding point, as bindless texture is shared
// between all kind of textures: 1d, 2d, 3d.
layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform usampler2D global_utextures[];

layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform sampler3D global_textures_3d[];

layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform usampler3D global_utextures_3d[];

layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform samplerCube global_textures_cubemaps[];

layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform samplerCubeArray global_textures_cubemaps_array[];

// Writeonly images do not need format in layout
layout( set = GLOBAL_SET, binding = BINDLESS_IMAGES ) writeonly uniform image2D global_images_2d[];

layout( set = GLOBAL_SET, binding = BINDLESS_IMAGES ) writeonly uniform image3D global_images_3d[];

layout( set = GLOBAL_SET, binding = BINDLESS_IMAGES ) writeonly uniform uimage2D global_uimages_2d[];

layout( set = GLOBAL_SET, binding = BINDLESS_IMAGES ) writeonly uniform uimage3D global_uimages_3d[];

// Utility methods ///////////////////////////////////////////////////////

vec4 texture_bindless_2d(uint texture_id, vec2 uv) {
    return texture( global_textures[nonuniformEXT(texture_id)], uv );
}

vec4 texture_bindless_2dlod(uint texture_id, vec2 uv, float lod) {
    return textureLod( global_textures[nonuniformEXT(texture_id)], uv, lod );
}

ivec2 texture_bindless_size2d(uint texture_id) {
    return textureSize(global_textures[nonuniformEXT(texture_id)], 0);
}

vec4 texture_bindless_3d(uint texture_id, vec3 uvw) {
    return texture( global_textures_3d[nonuniformEXT(texture_id)], uvw );
}

#endif // IDRA_BINDLESS
