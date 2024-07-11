#include "kernel/platform.hpp"

// Use this file to experiment an interface for threads, memory management and logging as foundations
#include <thread>
#include <atomic>
#include <future>
#include <chrono>

#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <strsafe.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <chrono>

#include <initializer_list>

#include "kernel/input.hpp"

#include "imgui.h"

#include "kernel/time.hpp"
#include "kernel/assert.hpp"
#include "kernel/string_view.hpp"
#include "kernel/utf.hpp"
#include "kernel/string.hpp"
#include "kernel/allocator.hpp"
#include "kernel/memory.hpp"
#include "kernel/numerics.hpp"
#include "kernel/array.hpp"
#include "kernel/hash_map.hpp"
#include "kernel/blob.hpp"
#include "kernel/thread.hpp"
#include "kernel/task_manager.hpp"
#include "kernel/pool.hpp"
#include "kernel/file.hpp"

#include "application/game_camera.hpp"
#include "application/window.hpp"
#include "application/application.hpp"

#include "gpu/gpu_device.hpp"
#include "gpu/idra_imgui.hpp"

#include "graphics/debug_renderer.hpp"
#include "graphics/graphics_asset_loaders.hpp"
#include "graphics/graphics_blueprints.hpp"
#include "graphics/sprite_render_system.hpp"

#include "imgui/widgets.hpp"

#include "tools/shader_compiler/shader_compiler.hpp"
#include "tools/asset_compiler/asset_compiler.hpp"

//#include "kernel/memory_hooks.hpp" // this still breaks some external libraries, add with caution.

#include "cglm/struct/euler.h"
#include "cglm/struct/affine.h"
#include "cglm/struct/mat3.h"
#include "cglm/struct/cam.h"

// Include glsl file to get the atmosphere struct
#define vec3 vec3s
#define vec4 vec4s
#define mat4 mat4s
#define uint u32
#include "../../data/shaders/atmospheric_scattering/definitions.glsl"
#undef vec3
#undef vec4
#undef mat4
#undef uint


namespace idra {

struct DevGames2024Demo {

    void                        create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase );
    void                        destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase );

    void                        main();

    // Utility methods

    static void                 setup_earth_atmosphere( AtmosphereParameters& info, f32 length_unit_in_meters );

    GpuDevice*                  gpu = nullptr;

    // Ocean methods
    void                        generate_wave_mesh( ImGui::ImGuiRenderView& window );
    void                        generate_wave_textures();

    // Resources used by demo

    // Atmospheric scattering
    ShaderAsset*                transmittance_lut_shader;
    PipelineHandle              transmittance_lut_pso;

    ShaderAsset*                multiscattering_lut_shader;
    PipelineHandle              multiscattering_lut_pso;

    ShaderAsset*                aerial_perspective_shader;
    PipelineHandle              aerial_perspective_pso;

    ShaderAsset*                sky_lut_shader;
    PipelineHandle              sky_lut_pso;

    ShaderAsset*                sky_apply_shader;
    PipelineHandle              sky_apply_pso;

    // Shared
    DescriptorSetLayoutHandle   shared_dsl;
    DescriptorSetHandle         shared_ds;

    SamplerHandle               sampler_clamp;
    SamplerHandle               sampler_clamp_edge;
    SamplerHandle               sampler_nearest;
    SamplerHandle               sampler_repeat;

    // Textures
    TextureHandle               transmittance_lut;
    TextureHandle               multiscattering_lut;
    TextureHandle               sky_view_lut;
    TextureHandle               irradiance_texture;
    TextureHandle               aerial_perspective_texture;
    TextureHandle               aerial_perspective_texture_debug;

    AtmosphereParameters        atmosphere_parameters;

    // TODO: external dependency
    vec3s                       sun_direction;
    u32                         aerial_perspective_debug_slice = 16;

    // Ocean
    ShaderAsset*                ocean_bruneton_render_shader;
    PipelineHandle              ocean_bruneton_render_pso;

    ShaderAsset*                skymap_shader;
    PipelineHandle              skymap_pso;

    DescriptorSetLayoutHandle   ocean_bruneton_dsl;
    DescriptorSetHandle         ocean_bruneton_ds;

    DescriptorSetLayoutHandle   skymap_dsl;
    DescriptorSetHandle         skymap_ds;

    BufferHandle                ocean_grid_index_buffer;
    BufferHandle                ocean_grid_buffer;
    u32                         ocean_grid_vertex_count;
    u32                         ocean_grid_index_count;
    f32                         last_width = 0.0f;
    f32                         last_height = 0.0f;
    vec2s*                      ocean_vertices = nullptr;
    u16*                        ocean_indices = nullptr;

    TextureHandle               wave_texture;
    f32*                        irradiance_data;
    vec4s*                      waves_data;

    TextureHandle               inscatter_texture;
    f32*                        inscatter_data;

    TextureHandle               noise_texture;
    u8*                         noise_data;

    TextureHandle               skymap_texture;

    f32                         grid_size = 8.0f;
    f32                         lambda_min = 0.02f;
    f32                         lambda_max = 30.0f;
    u16                         nb_waves = 60;
    f32                         wave_dispersion = 1.25f;
    f32                         wave_max_height = 0.32f;
    f32                         U0 = 10.0f;
    f32                         wave_direction = 2.4;
    f32                         hdr_exposure = 0.4;
    f32                         nyquist_min = 1.0;
    f32                         nyquist_max = 1.5;
    vec4s                       sea_color = { 10.0 / 255.0, 40.0 / 255.0, 120.0 / 255.0, 0.1 };
    f32                         mean_height = 0.0;
    f32                         sigma_Xsq = 0.0;
    f32                         sigma_Ysq = 0.0;

    f32                         octaves = 10.0;
    f32                         lacunarity = 2.2;
    f32                         gain = 0.7;
    f32                         norm = 0.5;
    f32                         clamp1 = -0.15;
    f32                         clamp2 = 0.2;
    vec4s                       cloudColor = { 1.0, 1.0, 1.0, 1.0 };

}; // struct DevGames2024Demo


// From Bruneton Ocean implementation //////////////////////////////////////
struct OceanConstantsBruneton {
    mat4s   screenToCamera; // screen space to camera space
    mat4s   cameraToWorld; // camera space to world space
    mat4s   worldToScreen; // world space to screen space
    float   worldToWind[8]; // world space to wind space
    float   windToWorld[8]; // wind space to world space

    vec3s   worldCamera; // camera position in world space
    f32     nbWaves; // number of waves

    vec3s   worldSunDir; // sun direction in world space
    f32     heightOffset; // so that surface height is centered around z = 0

    vec2s   sigmaSqTotal; // total x and y variance in wind space
    f32     time; // current time
    f32     nyquistMin; // Nmin parameter

    // grid cell size in pixels, angle under which a grid cell is seen,
    // and parameters of the geometric series used for wavelengths
    vec4s   lods;

    vec3s   seaColor; // sea bottom color
    f32     nyquistMax; // Nmax parameter

    f32     hdrExposure;
    vec3s   padding002_;

}; // struct OceanConstants

struct SkymapConstants {
    vec3s worldSunDir; // sun direction in world space
    f32 octaves;

    vec4s cloudsColor;

    f32 lacunarity;
    f32 gain;
    f32 norm;
    f32 clamp1;

    f32 clamp2;
    f32 texture_width;
    f32 texture_height;
    u32 destination_texture;
}; // struct SkymapConstants

// ----------------------------------------------------------------------------
// MESH GENERATION
// ----------------------------------------------------------------------------

void DevGames2024Demo::generate_wave_mesh( ImGui::ImGuiRenderView& window )
{
    f32 camera_theta = 0.0f; // TODO(marco): read this from camera

    f32 horizon = tan(camera_theta);
    f32 s = idra::min(1.1f, 0.5f + horizon * 0.5f);

    f32 vmargin = 0.1;
    f32 hmargin = 0.1;

    ImVec2 render_size = window.get_size();
    f32 width = render_size.x;
    f32 height = render_size.y;

    if ( width == last_width && height == last_height ) {
        return;
    }

    idra::Allocator* app_allocator = idra::g_memory->get_current_allocator();

    if ( ocean_vertices ) {
        ifree( ocean_vertices, app_allocator );
        ifree( ocean_indices, app_allocator );
    }

    last_width = width;
    last_height = height;

    if ( ocean_grid_buffer.is_valid() ) {
        gpu->destroy_buffer( ocean_grid_buffer );
        gpu->destroy_buffer( ocean_grid_index_buffer );
    }

    u32 max_vertex_count = int(ceil(height * (s + vmargin) / grid_size) + 5) * int(ceil(width * (1.0 + 2.0 * hmargin) / grid_size) + 5);
    ocean_vertices = ( vec2s* )ialloc( max_vertex_count * sizeof( vec2s ), app_allocator );

    ocean_grid_vertex_count = 0;
    int nx = 0;
    for (f32 j = height * s - 0.1; j > -height * vmargin - grid_size; j -= grid_size) {
        nx = 0;
        for (f32 i = -width * hmargin; i < width * (1.0 + hmargin) + grid_size; i += grid_size) {
            ocean_vertices[ocean_grid_vertex_count++] = vec2s{ -1.0f + 2.0f * i / width, -1.0f + 2.0f * j / height };
            nx++;
        }
    }

    ocean_grid_buffer = gpu->create_buffer({
        .type = BufferUsage::Vertex_mask, .usage = ResourceUsageType::Stream,
        .size = ocean_grid_vertex_count * sizeof( vec2s ), .persistent = 1, .device_only = 0, .initial_data = ocean_vertices,
        .debug_name = "VB_wave_grid"
    });

    u32 max_index_count = 6 * int(ceil(height * (s + vmargin) / grid_size) + 4) * int(ceil(width * (1.0 + 2.0 * hmargin) / grid_size) + 4);
    ocean_indices = ( u16* )ialloc( max_index_count * sizeof( u16 ), app_allocator );

    int nj = 0;
    ocean_grid_index_count = 0;
    for (f32 j = height * s - 0.1; j > -height * vmargin; j -= grid_size) {
        int ni = 0;
        for (f32 i = -width * hmargin; i < width * (1.0 + hmargin); i += grid_size) {
            ocean_indices[ocean_grid_index_count++] = ni + (nj + 1) * nx;
            ocean_indices[ocean_grid_index_count++] = (ni + 1) + (nj + 1) * nx;
            ocean_indices[ocean_grid_index_count++] = (ni + 1) + nj * nx;
            ocean_indices[ocean_grid_index_count++] = (ni + 1) + nj * nx;
            ocean_indices[ocean_grid_index_count++] = ni + (nj + 1) * nx;
            ocean_indices[ocean_grid_index_count++] = ni + nj * nx;
            ni++;
        }
        nj++;
    }

    ocean_grid_index_buffer = gpu->create_buffer({
        .type = BufferUsage::Index_mask, .usage = ResourceUsageType::Stream,
        .size = ocean_grid_index_count * sizeof( u16 ), .persistent = 1, .device_only = 0, .initial_data = ocean_indices,
        .debug_name = "VB_wave_index_grid"
    });
}

// ----------------------------------------------------------------------------
// WAVES GENERATION
// ----------------------------------------------------------------------------

static int lrandom(int *seed)
{
    *seed = (*seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return *seed;
}

static f32 frandom(int *seed)
{
    int r = lrandom(seed) >> (31 - 24);
    return r / (f32)(1 << 24);
}

static f32 grandom(f32 mean, f32 stdDeviation, int *seed)
{
    f32 x1, x2, w, y1;
    static f32 y2;
    static int use_last = 0;

    if (use_last) {
        y1 = y2;
        use_last = 0;
    } else {
        do {
            x1 = 2.0f * frandom(seed) - 1.0f;
            x2 = 2.0f * frandom(seed) - 1.0f;
            w  = x1 * x1 + x2 * x2;
        } while (w >= 1.0f);
        w  = sqrt((-2.0f * log(w)) / w);
        y1 = x1 * w;
        y2 = x2 * w;
        use_last = 1;
    }
	return mean + y1 * stdDeviation;
}

#define srnd() (2*frandom(&seed) - 1)

void DevGames2024Demo::generate_wave_textures()
{
    int seed = 1234567;
    f32 min = log(lambda_min) / log(2.0f);
    f32 max = log(lambda_max) / log(2.0f);

    f32 heightVariance = 0.0;
    f32 amplitudeMax = 0.0;

    sigma_Xsq = 0.0f;
    sigma_Ysq = 0.0f;
    mean_height = 0.0f;

    idra::Allocator* app_allocator = idra::g_memory->get_current_allocator();
    waves_data = ( vec4s* )ialloc( ( sizeof( vec4s ) * nb_waves  ), app_allocator );

	#define nbAngles 5 // even
	#define angle(i) (1.5*(((i)%nbAngles)/(f32)(nbAngles/2)-1))
	#define dangle() (1.5/(f32)(nbAngles/2))

	f32 Wa[nbAngles]; // normalised gaussian samples
    int index[nbAngles]; // to hash angle order
    f32 s=0;
    for (int i = 0; i < nbAngles; i++) {
        index[i] = i;
        f32 a = angle(i);
        s += Wa[i] = exp(-0.5*a*a);
    }
    for (int i = 0; i < nbAngles; i++) {
        Wa[i] /= s;
    }

    for (int i = 0; i < nb_waves; ++i) {
        f32 x = i / (nb_waves - 1.0f);

        f32 lambda = pow(2.0f, (1.0f - x) * min + x * max);
        f32 ktheta = grandom(0.0f, 1.0f, &seed) * wave_dispersion;
        f32 knorm = 2.0f * M_PI / lambda;
        f32 omega = sqrt(9.81f * knorm);
        f32 amplitude;

        f32 step = (max-min) / (nb_waves-1); // dlambda/di
        f32 omega0 = 9.81 / U0;
        if ((i%(nbAngles)) == 0) { // scramble angle ordre
            for (int k = 0; k < nbAngles; k++) {   // do N swap in indices
                int n1 = lrandom(&seed)%nbAngles, n2 = lrandom(&seed)%nbAngles, n;
                n = index[n1];
				index[n1] = index[n2];
				index[n2] = n;
            }
        }
        ktheta = wave_dispersion * (angle(index[(i)%nbAngles]) + 0.4*srnd()*dangle());
        ktheta *= 1.0 / (1.0 + 40.0*pow(omega0/omega,4));
        amplitude = (8.1e-3*9.81*9.81) / pow(omega,5) * exp(-0.74*pow(omega0/omega,4));
        amplitude *= 0.5*sqrt(2*M_PI*9.81/lambda)*nbAngles*step;
        amplitude = 3*wave_max_height*sqrt(amplitude);

        if (amplitude > 1.0f / knorm) {
            amplitude = 1.0f / knorm;
        } else if (amplitude < -1.0f / knorm) {
            amplitude = -1.0f / knorm;
        }

        waves_data[i].x = amplitude;
        waves_data[i].y = omega;
        waves_data[i].z = knorm * cos(ktheta);
        waves_data[i].w = knorm * sin(ktheta);
        sigma_Xsq += pow(cos(ktheta), 2.0f) * (1.0 - sqrt(1.0 - knorm * knorm * amplitude * amplitude));
        sigma_Ysq += pow(sin(ktheta), 2.0f) * (1.0 - sqrt(1.0 - knorm * knorm * amplitude * amplitude));
        mean_height -= knorm * amplitude * amplitude * 0.5f;
        heightVariance += amplitude * amplitude * (2.0f - knorm * knorm * amplitude * amplitude) * 0.25f;
        amplitudeMax += fabs(amplitude);
    }

    #undef nbAngles
    #undef angle
    #undef dangle

    f32 var = 4.0f;
    amplitudeMax = 2.0f * var * sqrt(heightVariance);

    wave_texture = gpu->create_texture( {
        .width = nb_waves, .height = 1, .depth = 1, .array_layer_count = 1,
        .mip_level_count = 1, .flags = TextureFlags::Default_mask,
        .format = TextureFormat::R32G32B32A32_FLOAT, .type = TextureType::Texture1D,
        .sampler = sampler_nearest, .initial_data = waves_data, .debug_name = "wave_texture" } );
}
//
//

void DevGames2024Demo::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Startup ) {

        // Atmospheric scattering
        setup_earth_atmosphere( atmosphere_parameters, 1000.f );

        ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

        transmittance_lut_shader = shader_loader->compile_compute( {},
                                                                   { "platform.h", "atmospheric_scattering/definitions.glsl",
                                                                   "atmospheric_scattering/functions.glsl",
                                                                   "atmospheric_scattering/sky_common.h" },
                                                                   "atmospheric_scattering/transmittance_lut.comp",
                                                                   "transmittance_lut" );

        multiscattering_lut_shader = shader_loader->compile_compute( {},
                                                                     { "platform.h", "atmospheric_scattering/definitions.glsl",
                                                                     "atmospheric_scattering/functions.glsl",
                                                                     "atmospheric_scattering/sky_common.h" },
                                                                     "atmospheric_scattering/multi_scattering.comp",
                                                                     "multiscattering_lut" );

        aerial_perspective_shader = shader_loader->compile_compute( { "MULTISCATAPPROX_ENABLED" },
                                                                    { "platform.h", "atmospheric_scattering/definitions.glsl",
                                                                    "atmospheric_scattering/functions.glsl",
                                                                    "atmospheric_scattering/sky_common.h" },
                                                                    "atmospheric_scattering/aerial_perspective.comp",
                                                                    "aerial_perspective" );

        sky_lut_shader = shader_loader->compile_compute( { "MULTISCATAPPROX_ENABLED" },
                                                         { "platform.h", "atmospheric_scattering/definitions.glsl",
                                                         "atmospheric_scattering/functions.glsl",
                                                         "atmospheric_scattering/sky_common.h" },
                                                         "atmospheric_scattering/sky_lut.comp",
                                                         "sky_lut" );

        sky_apply_shader = shader_loader->compile_graphics(
            { "MULTISCATAPPROX_ENABLED" },
            { "platform.h", "atmospheric_scattering/definitions.glsl",
            "atmospheric_scattering/functions.glsl",
            "atmospheric_scattering/sky_common.h" },
            "fullscreen_triangle.vert",
            "atmospheric_scattering/sky_apply.frag",
            "sky_apply" );


        sampler_clamp = gpu->create_sampler( {
            .min_filter = TextureFilter::Linear, .mag_filter = TextureFilter::Linear,
            .mip_filter = SamplerMipmapMode::Linear, .address_mode_u = SamplerAddressMode::Clamp_Border,
            .address_mode_v = SamplerAddressMode::Clamp_Border, .address_mode_w = SamplerAddressMode::Clamp_Border,
            .debug_name = "atmospheric scattering clamp sampler" } );

        sampler_clamp_edge = gpu->create_sampler( {
            .min_filter = TextureFilter::Linear, .mag_filter = TextureFilter::Linear,
            .mip_filter = SamplerMipmapMode::Linear, .address_mode_u = SamplerAddressMode::Clamp_Edge,
            .address_mode_v = SamplerAddressMode::Clamp_Edge, .address_mode_w = SamplerAddressMode::Clamp_Edge,
            .debug_name = "clamp sampler edge" } );

        sampler_nearest = gpu->create_sampler( {
            .min_filter = TextureFilter::Nearest, .mag_filter = TextureFilter::Nearest,
            .mip_filter = SamplerMipmapMode::Nearest, .address_mode_u = SamplerAddressMode::Clamp_Edge,
            .address_mode_v = SamplerAddressMode::Clamp_Edge, .address_mode_w = SamplerAddressMode::Clamp_Edge,
            .debug_name = "waves clamp sampler" } );

        // TODO(marco): anysotropy
        sampler_repeat = gpu->create_sampler( {
            .min_filter = TextureFilter::Linear, .mag_filter = TextureFilter::Linear,
            .mip_filter = SamplerMipmapMode::Linear, .address_mode_u = SamplerAddressMode::Repeat,
            .address_mode_v = SamplerAddressMode::Repeat, .address_mode_w = SamplerAddressMode::Repeat,
            .debug_name = "noise repeat sampler" } );

        // TODO(marco): compute this at runtime and use 16F
        idra::Allocator* app_allocator = idra::g_memory->get_current_allocator();

        // Use scratch/temp allocator to read from file.
        idra::BookmarkAllocator* scratch_allocator = g_memory->get_thread_allocator();
        sizet scratch_marker = scratch_allocator->get_marker();
        Span<char> irradiance_file_data_raw = file_read_allocate( "data/textures/irradiance.raw", scratch_allocator );
        f32* irradiance_file_data = ( f32* )irradiance_file_data_raw.data;

        irradiance_data = ( f32* )ialloc( ( 64 * 16 * 4 * sizeof( f32 ) ), app_allocator );
        for ( u32 i = 0; i <  64 * 16; i++ )
        {
            irradiance_data[ i * 4 ] = irradiance_file_data[ i * 3 ];
            irradiance_data[ i * 4 + 1 ] = irradiance_file_data[ i * 3 + 1 ];
            irradiance_data[ i * 4 + 2 ] = irradiance_file_data[ i * 3 + 2 ];
            irradiance_data[ i * 4 + 3 ] = 1.0f;
        }

        irradiance_texture = gpu->create_texture( {
            .width = 64, .height = 16, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R32G32B32A32_FLOAT, .type = TextureType::Texture2D,
            .sampler = sampler_clamp_edge, .initial_data = irradiance_data, .debug_name = "irradiance_texture" } );

        scratch_allocator->free_marker( scratch_marker );

        generate_wave_textures();

        transmittance_lut = gpu->create_texture( {
            .width = 256, .height = 64, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .sampler = sampler_clamp, .debug_name = "transmittance_lut" } );

        multiscattering_lut = gpu->create_texture( {
            .width = 32, .height = 32, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .debug_name = "multi_scattering_lut" } );

        sky_view_lut = gpu->create_texture( {
            .width = 192, .height = 108, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R11G11B10_FLOAT, .type = TextureType::Texture2D,
            .sampler = sampler_clamp, .debug_name = "sky_view_lut" } );

        aerial_perspective_texture = gpu->create_texture( {
            .width = 32, .height = 32, .depth = 32, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture3D,
            .debug_name = "aerial_perspective_texture" } );

        aerial_perspective_texture_debug = gpu->create_texture( {
            .width = 32, .height = 32, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .debug_name = "aerial_perspective_texture_debug" } );

        skymap_texture =  gpu->create_texture( {
            .width = 512, .height = 512, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R16G16B16A16_FLOAT, .type = TextureType::Texture2D,
            .sampler = sampler_clamp_edge, .debug_name = "skymap_texture" } );

        shared_dsl = gpu->create_descriptor_set_layout( {
            .dynamic_buffer_bindings = { 0 },
            .debug_name = "atmospheric_scattering_dsl" } );

        shared_ds = gpu->create_descriptor_set( {
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( AtmosphereParameters )}},
            .layout = shared_dsl,
            .debug_name = "atmospheric_scattering_ds" } );

        // Ocean
        ocean_bruneton_render_shader = shader_loader->compile_graphics(
            {},
            { "ocean_bruneton/ocean.h", "ocean_bruneton/common.h" },
            "ocean_bruneton/ocean.vert",
            "ocean_bruneton/ocean.frag",
            "ocean_render_bruneton" );

        ocean_bruneton_dsl = gpu->create_descriptor_set_layout( {
            .bindings = {
                { .type = DescriptorType::Texture, .start = 1, .count = 1, .name = "wave_sampler" },
                { .type = DescriptorType::Texture, .start = 2, .count = 1, .name = "sky_sampler" },
                { .type = DescriptorType::Texture, .start = 3, .count = 1, .name = "sky_irradiance_sampler" },
                { .type = DescriptorType::Texture, .start = 4, .count = 1, .name = "transmittance_sampler" },
            },
            .dynamic_buffer_bindings = { 0 },
            .debug_name = "ocean_bruneton_dsl" } );

        ocean_bruneton_ds = gpu->create_descriptor_set( {
            .textures = {
                { .texture = wave_texture, .binding = 1 },
                { .texture = skymap_texture, .binding = 2 },
                { .texture = irradiance_texture, .binding = 3 },
                { .texture = transmittance_lut, .binding = 4 },
            },
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( OceanConstantsBruneton )}},
            .layout = ocean_bruneton_dsl,
            .debug_name = "ocean_bruneton_ds" } );

        FileHandle noise_file = file_open_for_read( "data/textures/noise.pgm" );
        long noise_size = fs_file_get_size( noise_file );
        noise_data = ( u8* )ialloc( noise_size, app_allocator );
        file_read<u8>( noise_file, noise_data, noise_size );

        noise_texture = gpu->create_texture( {
            .width = 512, .height = 512, .depth = 1, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R8_UNORM, .type = TextureType::Texture2D,
            .sampler = sampler_repeat, .initial_data = noise_data + 38, .debug_name = "noise_texture" } );

        file_close( noise_file );

        int inscatter_res = 64;
        int inscatter_nr = inscatter_res / 2;
        int inscatter_nv = inscatter_res * 2;
        int inscatter_nb = inscatter_res / 2;
        int inscatter_na = 8;

        FileHandle inscatter_file = file_open_for_read( "data/textures/inscatter.raw" );
        long inscatter_size = fs_file_get_size( inscatter_file );
        long inscatter_read_size = inscatter_nr * inscatter_nv * inscatter_nb * inscatter_na * 4 * sizeof( f32 );
        // iassert( inscatter_size == inscatter_read_size ); // TODOO(marco): why these don't match?
        inscatter_data = ( f32* )ialloc(inscatter_read_size, app_allocator );

        file_read<f32>( inscatter_file, inscatter_data, inscatter_read_size);

        inscatter_texture = gpu->create_texture( {
            .width = ( u16 )( inscatter_na * inscatter_nb ), .height = ( u16 )inscatter_nv, .depth = ( u16 )inscatter_nr, .array_layer_count = 1,
            .mip_level_count = 1, .flags = TextureFlags::Compute_mask | TextureFlags::Default_mask,
            .format = TextureFormat::R32G32B32A32_FLOAT, .type = TextureType::Texture3D,
            .sampler = sampler_clamp_edge, .initial_data = inscatter_data, .debug_name = "inscatter_texture" } );

        file_close( inscatter_file );

        skymap_shader = shader_loader->compile_compute(
            {},
            { "platform.h", "ocean_bruneton/common.h" },
            "ocean_bruneton/skymap.comp",
            "skymap" );

        skymap_dsl = gpu->create_descriptor_set_layout( {
            .bindings = {
                { .type = DescriptorType::Texture, .start = 1, .count = 1, .name = "sky_irradiance_sampler" },
                { .type = DescriptorType::Texture, .start = 2, .count = 1, .name = "noise_sampler" },
                { .type = DescriptorType::Texture, .start = 3, .count = 1, .name = "transmittance_sampler" },
                { .type = DescriptorType::Texture, .start = 4, .count = 1, .name = "inscatter_sampler" },
            },
            .dynamic_buffer_bindings = { 0 },
            .debug_name = "skymap_dsl" } );

        skymap_ds = gpu->create_descriptor_set( {
            .textures = {
                { .texture = irradiance_texture, .binding = 1 },
                { .texture = noise_texture, .binding = 2 },
                { .texture = transmittance_lut, .binding = 3 },
                { .texture = inscatter_texture, .binding = 4 },
            },
            .dynamic_buffer_bindings = {{.binding = 0, .size = sizeof( SkymapConstants )}},
            .layout = skymap_dsl,
            .debug_name = "skymap_ds" } );
    }

    // Update dependent assets/resources
    // NOTE: shaders are already reloaded, and just the shader handle is modified.
    // Just need to create the pipelines.

    // Atmospheric scattering
    transmittance_lut_pso = gpu->create_compute_pipeline( {
        .shader = transmittance_lut_shader->shader,
        .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "transmittance_lut_pso" } );

    multiscattering_lut_pso = gpu->create_compute_pipeline( {
        .shader = multiscattering_lut_shader->shader,
        .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "transmittance_lut_pso" } );

    aerial_perspective_pso = gpu->create_compute_pipeline( {
        .shader = aerial_perspective_shader->shader,
        .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "aerial_perspective_pso" } );

    sky_lut_pso = gpu->create_compute_pipeline( {
        .shader = sky_lut_shader->shader,
        .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, shared_dsl },
        .debug_name = "sky_lut_pso" } );

    sky_apply_pso = gpu->create_graphics_pipeline( {
            .rasterization = {},
            .depth_stencil = {},
            .blend_state = {.blend_states = {{.source_color = Blend::SrcAlpha,
                                              .destination_color = Blend::InvSrcAlpha,
                                              .color_operation = BlendOperation::Add,
                                               } } },
            .vertex_input = {},
            .shader = sky_apply_shader->shader,
            .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, shared_dsl },
            .viewport = {},
            .color_formats = { gpu->swapchain_format },
            .depth_format = TextureFormat::D32_FLOAT,
            .debug_name = "sky_apply_pso" } );

    // Ocean
    ocean_bruneton_render_pso = gpu->create_graphics_pipeline( {
            .rasterization = { .fill = FillMode::Solid },
            .depth_stencil = { .depth_comparison = ComparisonFunction::Less,
                                .depth_enable = 1, .depth_write_enable = 1 },
            .blend_state = {},
            .vertex_input = {
                .vertex_streams{ { .binding = 0, .stride = 8, .input_rate = VertexInputRate::PerVertex } },
                .vertex_attributes{ { .location = 0, .binding = 0, .offset = 0, .format = VertexComponentFormat::Float2 } }
            },
            .shader = ocean_bruneton_render_shader->shader,
            .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, ocean_bruneton_dsl },
            .viewport = {},
            .color_formats = { gpu->swapchain_format },
            .depth_format = TextureFormat::D32_FLOAT,
            .debug_name = "ocean_bruneton_render_pso" } );

    skymap_pso = gpu->create_compute_pipeline( {
        .shader = skymap_shader->shader,
        .descriptor_set_layouts = { gpu->bindless_descriptor_set_layout, skymap_dsl },
        .debug_name = "skymap_pso" } );
}

void DevGames2024Demo::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    // Atmospheric scattering
    // Destroy only the psos and return.
    gpu->destroy_pipeline( transmittance_lut_pso );
    gpu->destroy_pipeline( multiscattering_lut_pso );
    gpu->destroy_pipeline( aerial_perspective_pso );
    gpu->destroy_pipeline( sky_lut_pso );
    gpu->destroy_pipeline( sky_apply_pso );
    gpu->destroy_pipeline( ocean_bruneton_render_pso );
    gpu->destroy_pipeline( skymap_pso );

    if ( phase == AssetDestructionPhase::Reload ) {
        return;
    }

    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

    shader_loader->unload( transmittance_lut_shader );
    shader_loader->unload( multiscattering_lut_shader );
    shader_loader->unload( aerial_perspective_shader );
    shader_loader->unload( sky_lut_shader );
    shader_loader->unload( sky_apply_shader );
    shader_loader->unload( ocean_bruneton_render_shader );
    shader_loader->unload( skymap_shader );

    gpu->destroy_sampler( sampler_clamp );
    gpu->destroy_sampler( sampler_clamp_edge );
    gpu->destroy_sampler( sampler_nearest );
    gpu->destroy_sampler( sampler_repeat );

    gpu->destroy_texture( transmittance_lut );
    gpu->destroy_texture( multiscattering_lut );
    gpu->destroy_texture( aerial_perspective_texture );
    gpu->destroy_texture( aerial_perspective_texture_debug );
    gpu->destroy_texture( sky_view_lut );
    gpu->destroy_texture( wave_texture );
    gpu->destroy_texture( irradiance_texture );
    gpu->destroy_texture( inscatter_texture );
    gpu->destroy_texture( noise_texture );
    gpu->destroy_texture( skymap_texture );

    gpu->destroy_buffer( ocean_grid_buffer );
    gpu->destroy_buffer( ocean_grid_index_buffer );

    gpu->destroy_descriptor_set_layout( shared_dsl );
    gpu->destroy_descriptor_set_layout( ocean_bruneton_dsl );
    gpu->destroy_descriptor_set_layout( skymap_dsl );
    gpu->destroy_descriptor_set( shared_ds );
    gpu->destroy_descriptor_set( ocean_bruneton_ds );
    gpu->destroy_descriptor_set( skymap_ds );

    // TODO(marco): we should probably take a copy of the upload texture data so that users don't have to worry
    // about keeping the data around
    idra::Allocator* app_allocator = idra::g_memory->get_current_allocator();
    ifree( waves_data, app_allocator );
    ifree( irradiance_data, app_allocator );
    ifree( ocean_vertices, app_allocator );
    ifree( ocean_indices, app_allocator );
    ifree( inscatter_data, app_allocator );
    ifree( noise_data, app_allocator );
}

void DevGames2024Demo::main() {

    // Init services
    g_memory->init( ikilo( 5400 ), ikilo( 4200 ) );
    g_time->init();
    g_log->init( g_memory->get_resident_allocator() );

    // Asset compiler test
    asset_compiler_main( "../data", "data" );

    idra::TLSFAllocator tlsf_allocator{};
    tlsf_allocator.init( imega( 32 ) );

    idra::g_memory->set_current_allocator( &tlsf_allocator );

    InputSystem* input = InputSystem::init_system();

    // Window creation
    Window window;
    window.init( 1280, 720, "DevGames 2024 demo", nullptr, input );

    idra::Allocator* app_allocator = idra::g_memory->get_current_allocator();

    // GPU Device initalization.
    gpu = idra::GpuDevice::init_system( {
        .system_allocator = app_allocator,
        .os_window_handle = window.platform_handle,
        .shader_folder_path = "../data/shaders" } );

    // ImGui Service
    g_imgui->init( gpu, window.platform_handle );

    ImGui::ApplicationLogInit();
    ImGui::FPSInit();

    // Asset manager
    idra::AssetManager* asset_manager = idra::AssetManager::init_system();
    // Asset loaders
    idra::ShaderAssetLoader shader_loader;
    shader_loader.init( app_allocator, 32, asset_manager, gpu );

    idra::TextureAssetLoader texture_loader;
    texture_loader.init( app_allocator, 128, asset_manager, gpu );

    idra::TextureAtlasLoader atlas_loader;
    atlas_loader.init( app_allocator, 128, asset_manager, gpu );

    // Assign loaders
    asset_manager->set_loader( idra::ShaderAssetLoader::k_loader_index, &shader_loader );
    asset_manager->set_loader( idra::TextureAssetLoader::k_loader_index, &texture_loader );
    asset_manager->set_loader( idra::TextureAtlasLoader::k_loader_index, &atlas_loader );

    // Load assets!

    // First camera!
    idra::GameCamera game_camera;
    game_camera.camera.init_perpective( 0.1f, 1000.f, 60.f, gpu->swapchain_width * 1.f / gpu->swapchain_height );
    game_camera.camera.position = { 0, 2.0f, 0 };
    game_camera.init( true, 20.f, 6.f, 0.1f );

    // Render Systems
    idra::DebugRenderer debug_renderer( 2, 10000 );

    // Add all render systems
    idra::Array<idra::RenderSystemInterface*> render_systems;
    render_systems.init( app_allocator, 4 );

    render_systems.push( &debug_renderer );

    // Init render systems
    for ( u32 i = 0; i < render_systems.size; ++i ) {
        render_systems[ i ]->init( gpu, app_allocator );
        render_systems[ i ]->create_resources( asset_manager, idra::AssetCreationPhase::Startup );
    }

    create_resources( asset_manager, idra::AssetCreationPhase::Startup );

    // Render targets
    idra::TextureHandle game_rt = gpu->create_texture( {
        .width = ( u16 )gpu->swapchain_width, .height = ( u16 )gpu->swapchain_height, .depth = 1, .array_layer_count = 1,
        .mip_level_count = 1, .flags = idra::TextureFlags::Compute_mask | idra::TextureFlags::RenderTarget_mask,
        .format = gpu->swapchain_format, .type = idra::TextureType::Texture2D,
        .debug_name = "game_rt" } );

    idra::TextureHandle game_depth_rt = gpu->create_texture( {
        .width = ( u16 )gpu->swapchain_width, .height = ( u16 )gpu->swapchain_height, .depth = 1, .array_layer_count = 1,
        .mip_level_count = 1, .flags = idra::TextureFlags::RenderTarget_mask,
        .format = TextureFormat::D32_FLOAT, .type = idra::TextureType::Texture2D,
        .debug_name = "game_depth_rt" } );

    ImGui::ImGuiRenderView game_render_view;
    game_render_view.init( &game_camera, { game_rt, game_depth_rt }, gpu );

    bool quit_application = false;
    bool show_input_debug_ui = false;

    TimeTick begin_frame_tick = g_time->now();
    TimeTick absolute_begin_frame_tick = begin_frame_tick;

    const u32 game_view_index = 0;
    const u32 debug_view_index = 1;

    // Options
    bool show_ocean = true;
    bool show_debug_rendering = true;
    bool apply_atmospheric_scattering = true;

    // Ocean
    u32 ocean_num_subdivisions = 32;
    f32 ocean_uv_scale = 0.02f;
    f32 ocean_height_scale = 0.2f;

    f32 elapsed_time = 0.f;

    // Sun
    float sun_pitch = 0.45f, sun_yaw = 0;

    // Main loop!
    while ( window.is_running && !quit_application ) {
        // Frame begin
        window.handle_os_messages();
        input->update();

        if ( window.resized ) {

            game_camera.camera.set_aspect_ratio( window.width * 1.f / window.height );
            game_camera.camera.set_viewport_size( window.width, window.height );

            idra::SwapchainStatus::Enum swapchain_status = gpu->update_swapchain();
            if ( swapchain_status == idra::SwapchainStatus::NotReady ) {
                // TODO: imgui will need to be called here anyway
                continue;
            }

            window.resized = false;
        }

        gpu->new_frame();
        g_imgui->new_frame();

        // Check for game window resize
        game_render_view.check_resize( gpu, input );

        const TimeTick current_tick = g_time->now();
        f32 delta_time = ( f32 )g_time->convert_seconds( g_time->delta( current_tick, begin_frame_tick ) );
        begin_frame_tick = current_tick;

        elapsed_time += delta_time;

        // Re-center mouse
        if ( game_render_view.focus ) {
            game_camera.update( input, window.width, window.height, delta_time );
            // TODO: improve interface
            window.center_mouse( game_camera.mouse_dragging );
        }

        // Sun
        // Calculate sun direction
        const mat4s sun_rotation = glms_euler_xyz( { -sun_pitch, sun_yaw , 0 } );
        sun_direction = { sun_rotation.m02, sun_rotation.m12, sun_rotation.m22 };

        // Debug rendering test
        // View index is a way to dispatch line draws to different cameras
        debug_renderer.aabb( { -1,-1,-1 }, { 1,1,1 }, idra::Color::green(), game_view_index );

        // Frame update
        ImGui::DockSpaceOverViewport( ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );
        if ( ImGui::BeginMainMenuBar() ) {
            if ( ImGui::BeginMenu( "File" ) ) {
                //ShowExampleMenuFile();
                ImGui::MenuItem( "Input Debug UI", nullptr, &show_input_debug_ui );
                ImGui::MenuItem( "Quit", nullptr, &quit_application );
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if ( ImGui::Begin( "DevGames 2024" ) ) {

            if ( ImGui::Button( "Reload shaders" ) ) {

                for ( u32 i = 0; i < render_systems.size; ++i ) {
                    render_systems[ i ]->destroy_resources( asset_manager, idra::AssetDestructionPhase::Reload );
                }

                destroy_resources( asset_manager, idra::AssetDestructionPhase::Reload );

                asset_manager->get_loader<idra::ShaderAssetLoader>()->reload_assets();

                for ( u32 i = 0; i < render_systems.size; ++i ) {
                    render_systems[ i ]->create_resources( asset_manager, idra::AssetCreationPhase::Reload );
                }

                create_resources( asset_manager, idra::AssetCreationPhase::Reload );
            }

            ImGui::Checkbox( "Show Ocean", &show_ocean );
            ImGui::Checkbox( "Apply Atmospheric Scattering", &apply_atmospheric_scattering );
            ImGui::Checkbox( "Show Debug Rendering", &show_debug_rendering );

            ImGui::Separator();
            ImGui::SliderUint( "Ocean subdivisions", &ocean_num_subdivisions, 1, 256 );
            ImGui::SliderFloat( "Ocean UV Scale", &ocean_uv_scale, 0.01f, 1.0f );
            ImGui::SliderFloat( "Ocean Height Scale", &ocean_height_scale, 0.01f, 1.0f );
        }
        ImGui::End();

        if ( ImGui::Begin( "Atmospheric Scattering" ) ) {

            ImGui::Text( "Camera position %f,%f,%f", game_camera.camera.position.x, game_camera.camera.position.y, game_camera.camera.position.z );

            if ( ImGui::Button( "Reset camera position" ) ) {
                game_camera.camera.position = { 0.f, 2.f, 0.f };
                game_camera.target_movement = game_camera.camera.position;
            }

            ImGui::Text( "Camera near %f far %f", game_camera.camera.near_plane, game_camera.camera.far_plane );

            if ( ImGui::SliderFloat( "Camera Near", &game_camera.camera.near_plane, 0.001f, 32000.f ) ) {
                game_camera.camera.update_projection = true;
            }

            if ( ImGui::SliderFloat( "Camera Far", &game_camera.camera.far_plane, 0.001f, 32000.f ) ) {
                game_camera.camera.update_projection = true;
            }

            ImGui::SliderFloat( "Camera Movement Delta", &game_camera.movement_delta, 0.001f, 100.f );

            ImGui::SliderFloat( "Sun Pitch", &sun_pitch, -3.14f, 3.14f );
            ImGui::SliderFloat( "Sun Yaw", &sun_yaw, -3.14f, 3.14f );

            ImGui::Separator();
            ImVec2 rt_size = ImGui::GetContentRegionAvail();
            ImGui::SliderUint( "Aerial Perspective Debug Slice", &aerial_perspective_debug_slice, 0, 31 );
            ImGui::Image( transmittance_lut, { 256, 64 } );
            ImGui::Image( wave_texture, { ( f32 )nb_waves, 1 } );
            ImGui::Image( irradiance_texture, { 64, 16 } );
            ImGui::Image( multiscattering_lut, { 32 * 3, 32 * 3 } );
            ImGui::Image( aerial_perspective_texture_debug, { 256, 256 } );
            ImGui::Image( sky_view_lut, { 192 * 2, 108 * 2 } );
        }
        ImGui::End();

        if ( ImGui::Begin( "Screen space grid debugging" ) ) {
            ImGui::Text( "Camera position %f,%f,%f", game_camera.camera.position.x, game_camera.camera.position.y, game_camera.camera.position.z );

            ImGui::Text( "Camera focal %f", game_camera.camera.projection.raw[0][0] );
            ImGui::Text( "Camera aspect %f", game_camera.camera.projection.raw[1][1] );

            ImGui::Text( "Camera View 0 %f, %f, %f", game_camera.camera.view.raw[0][0], game_camera.camera.view.raw[0][1], game_camera.camera.view.raw[0][2] );
            ImGui::Text( "Camera View 1 %f, %f, %f", game_camera.camera.view.raw[1][0], game_camera.camera.view.raw[1][1], game_camera.camera.view.raw[1][2] );
            ImGui::Text( "Camera View 2 %f, %f, %f", game_camera.camera.view.raw[2][0], game_camera.camera.view.raw[2][1], game_camera.camera.view.raw[2][2] );
            ImGui::Text( "Camera View 3 %f, %f, %f", game_camera.camera.view.raw[3][0], game_camera.camera.view.raw[3][1], game_camera.camera.view.raw[3][2] );

            mat3s rotation{
                game_camera.camera.view.raw[0][0], game_camera.camera.view.raw[0][1], game_camera.camera.view.raw[0][2],
                game_camera.camera.view.raw[1][0], game_camera.camera.view.raw[1][1], game_camera.camera.view.raw[1][2],
                game_camera.camera.view.raw[2][0], game_camera.camera.view.raw[2][1], game_camera.camera.view.raw[2][2]
            };
            vec3s camera_w{ game_camera.camera.view.raw[3][0], game_camera.camera.view.raw[3][1], game_camera.camera.view.raw[3][2] };

            vec3s camera_rotation = glms_mat3_mulv( rotation, camera_w );
            ImGui::Text( "Camera Rotation %f, %f, %f", camera_rotation.raw[0], camera_rotation.raw[1], camera_rotation.raw[2] );
        }
        ImGui::End();

        if ( show_input_debug_ui ) {
            input->debug_ui();
        }

        ImGui::ApplicationLogDraw();

        game_render_view.draw( "Game View" );

        // Render
        idra::CommandBuffer* cb = gpu->acquire_new_command_buffer();

        cb->push_marker( "frame" );

        // Setup constants
        const mat4s scale_matrix = glms_scale_make( { 1.f, -1.f, 1.f } );
        vec3s left_handed_sun_direction = glms_mat4_mulv3( scale_matrix, sun_direction, 1.0f );

        // Atmospheric scattering
        u32 atmosphere_cb_offset = 0;
        AtmosphereParameters* atmosphere_params = gpu->dynamic_buffer_allocate<AtmosphereParameters>( &atmosphere_cb_offset );
        if ( atmosphere_params ) {
            Camera* camera = &game_camera.camera;
            memcpy( atmosphere_params, &atmosphere_parameters, sizeof( AtmosphereParameters ) );

            atmosphere_params->inverse_view_projection = glms_mat4_inv( camera->view_projection );
            atmosphere_params->inverse_projection = glms_mat4_inv( camera->projection );
            atmosphere_params->inverse_view = glms_mat4_inv( camera->view );
            atmosphere_params->camera_position = camera->position;// scaling breaks a lot of things glms_vec3_scale( camera->position, 1.001f );

            atmosphere_params->sun_direction = left_handed_sun_direction;
            atmosphere_params->mie_absorption = glms_vec3_maxv( glms_vec3_zero(), glms_vec3_sub( atmosphere_parameters.mie_extinction, atmosphere_parameters.mie_scattering ) );

            atmosphere_params->transmittance_lut_texture_index = transmittance_lut.index;
            atmosphere_params->aerial_perspective_texture_index = aerial_perspective_texture.index;
            atmosphere_params->aerial_perspective_debug_texture_index = aerial_perspective_texture_debug.index;
            atmosphere_params->aerial_perspective_debug_slice = aerial_perspective_debug_slice;
            atmosphere_params->sky_view_lut_texture_index = sky_view_lut.index;
            atmosphere_params->multiscattering_texture_index = multiscattering_lut.index;
            atmosphere_params->scene_color_texture_index = game_rt.index;
            atmosphere_params->scene_depth_texture_index = game_depth_rt.index;
        }

        generate_wave_mesh( game_render_view );

        vec3s world_sun_dir{ sin(sun_pitch) * cos(sun_yaw), sin(sun_pitch) * sin(sun_yaw), cos(sun_pitch) };

        u32 ocean_bruneton_cb_offset = 0;
        OceanConstantsBruneton* ocean_bruneton_constants = gpu->dynamic_buffer_allocate<OceanConstantsBruneton>( &ocean_bruneton_cb_offset );
        if ( ocean_bruneton_constants ) {

            mat2s world_to_wind{
                cos(wave_direction), sin(wave_direction),
                -sin(wave_direction), cos(wave_direction)
            };

            mat2s wind_to_world{
                cos(wave_direction), -sin(wave_direction),
                sin(wave_direction), cos(wave_direction)
            };

            float ch = 2.0f - mean_height;

            mat4s view = mat4s{
                0.0, -1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, -ch,
                -1.0, 0.0, 0.0, 0.0,
                0.0, 0.0, 0.0, 1.0
            };
            view = glms_rotate_x( view, 0.0f );
            view = glms_mat4_transpose( view );

            ImVec2 window_size = game_render_view.get_size();
            // mat4s proj = glms_perspective(glm_rad( 90.0 ), float(window_size.x) / float(window_size.y), 0.1 * ch, 1000000.0 * ch);
            // float f = 1.0f / tan(fovy * M_PI / 180.0f / 2);
            f32 f = 1.0f / tan(glm_rad( 45 ));
            f32 aspect = float(window_size.x) / float(window_size.y);
            f32 zNear = 0.1 * ch;
            f32 zFar = 1000000.0 * ch;
            mat4s proj = { f / aspect, 0, 0,                         0,
                            0,        f, 0,                         0,
                            0,        0, (zFar + zNear) / (zNear - zFar), (2*zFar*zNear) / (zNear - zFar),
                            0,        0, -1,                        0 };
            proj = glms_mat4_transpose( proj );

            vec3s world_camera{ 0.0, 0.0, ch };

            mat4s view_projection = glms_mat4_mul( proj, view );

            ocean_bruneton_constants->screenToCamera = glms_mat4_inv( proj );
            ocean_bruneton_constants->cameraToWorld = glms_mat4_inv( view );
            ocean_bruneton_constants->worldToScreen = view_projection;
            ocean_bruneton_constants->worldToWind[0] = cos(wave_direction);
            ocean_bruneton_constants->worldToWind[1] = sin(wave_direction);
            ocean_bruneton_constants->worldToWind[4] = -sin(wave_direction);
            ocean_bruneton_constants->worldToWind[5] = cos(wave_direction);
            ocean_bruneton_constants->windToWorld[0] = cos(wave_direction);
            ocean_bruneton_constants->windToWorld[1] = -sin(wave_direction);
            ocean_bruneton_constants->windToWorld[4] = sin(wave_direction);
            ocean_bruneton_constants->windToWorld[5] = cos(wave_direction);

            ocean_bruneton_constants->worldCamera = world_camera;
            ocean_bruneton_constants->nbWaves = nb_waves;

            ocean_bruneton_constants->worldSunDir = world_sun_dir;
            ocean_bruneton_constants->heightOffset = -mean_height;

            ocean_bruneton_constants->sigmaSqTotal = vec2s{ sigma_Xsq, sigma_Ysq };
            ocean_bruneton_constants->time = elapsed_time;
            ocean_bruneton_constants->nyquistMin = nyquist_min;

            ocean_bruneton_constants->lods = vec4s{
                grid_size,
                atan(2.0f / window.height) * grid_size, // angle under which a screen pixel is viewed from the camera * gridSize
                log(lambda_min) / log(2.0f),
                (nb_waves - 1.0f) / (log(lambda_max) / log(2.0f) -  log(lambda_min) / log(2.0f))
            };

            ocean_bruneton_constants->seaColor = glms_vec3_scale( vec3s{ sea_color.r, sea_color.g, sea_color.b }, sea_color.a );
            ocean_bruneton_constants->nyquistMax = nyquist_max;

            ocean_bruneton_constants->hdrExposure = hdr_exposure;
            ocean_bruneton_constants->padding002_ = vec3s{ 0, 0, 0 };
        }

        u32 skymap_cb_offset = 0;
        SkymapConstants* skymap_constants = gpu->dynamic_buffer_allocate<SkymapConstants>( &skymap_cb_offset);
        if ( skymap_constants ) {

            skymap_constants->worldSunDir = world_sun_dir; // sun direction in world space
            skymap_constants->octaves = octaves;

            skymap_constants->cloudsColor = cloudColor;

            skymap_constants->lacunarity = lacunarity;
            skymap_constants->gain = gain;
            skymap_constants->norm = norm;
            skymap_constants->clamp1 = clamp1;

            skymap_constants->clamp2 = clamp2;
            skymap_constants->texture_width = 512.0f;
            skymap_constants->texture_height = 512.0f;
            skymap_constants->destination_texture = skymap_texture.index;
        }

        // Atmospheric scattering: calculate LUTs
        {
            cb->push_marker( "atmospheric scattering" );

            // Transmittance //////////////////////////////////////////////////////
            cb->push_marker( "transmittance lut" );
            cb->submit_barriers( { {transmittance_lut, ResourceState::UnorderedAccess, 0, 1} },
                                 {  } );
            cb->bind_pipeline( transmittance_lut_pso );
            cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { atmosphere_cb_offset } );
            cb->dispatch_2d( 256, 64, 32, 32 );

            cb->submit_barriers( { {transmittance_lut, ResourceState::ShaderResource, 0, 1} }, {} );
            cb->pop_marker();

            // Multi-scattering ///////////////////////////////////////////////////
            cb->push_marker( "multiscattering lut" );
            cb->submit_barriers( { {multiscattering_lut, ResourceState::UnorderedAccess, 0, 1} },
                                 {  } );
            cb->bind_pipeline( multiscattering_lut_pso );
            cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { atmosphere_cb_offset } );
            cb->dispatch_2d( 32, 32, 1, 1 );

            cb->submit_barriers( { {multiscattering_lut, ResourceState::ShaderResource, 0, 1} }, {} );

            cb->pop_marker();

            // Aerial perspective /////////////////////////////////////////////////
            cb->push_marker( "aerial perspective" );
            cb->submit_barriers( { {aerial_perspective_texture, ResourceState::UnorderedAccess, 0, 1},
                                 {aerial_perspective_texture_debug, ResourceState::UnorderedAccess, 0, 1} },
                                 {  } );
            cb->bind_pipeline( aerial_perspective_pso );
            cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { atmosphere_cb_offset } );
            cb->dispatch_3d( 32, 32, 32, 8, 8, 1 );

            cb->submit_barriers( { {aerial_perspective_texture, ResourceState::ShaderResource, 0, 1},
                                 {aerial_perspective_texture_debug, ResourceState::UnorderedAccess, 0, 1} }, {} );
            cb->pop_marker();

            // Sky view ///////////////////////////////////////////////////////////
            cb->push_marker( "sky view" );
            cb->submit_barriers( { {sky_view_lut, ResourceState::UnorderedAccess, 0, 1} },
                                 {  } );
            cb->bind_pipeline( sky_lut_pso );
            cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { atmosphere_cb_offset } );

            cb->dispatch_2d( 192, 108, 32, 32 );

            cb->submit_barriers( { {sky_view_lut, ResourceState::ShaderResource, 0, 1} }, {} );
            cb->pop_marker();

            cb->pop_marker();
        }

        // Render game view
        cb->push_marker( "game render" );
        cb->submit_barriers( { {game_rt, idra::ResourceState::RenderTarget, 0, 1},
                             {game_depth_rt, idra::ResourceState::RenderTarget, 0, 1} }, {} );

        cb->begin_pass( { game_rt }, { LoadOperation::Clear }, { {0,0,0,0} }, game_depth_rt, LoadOperation::Clear, { .depth_value = 1.0f } );
        cb->set_framebuffer_scissor();
        cb->set_framebuffer_viewport();

        if ( apply_atmospheric_scattering ) {
            cb->push_marker( "sky apply" );

            cb->bind_pipeline( sky_apply_pso );
            cb->bind_descriptor_set( { cb->gpu_device->bindless_descriptor_set, shared_ds }, { atmosphere_cb_offset } );
            cb->draw( TopologyType::Triangle, 0, 3, 0, 1 );

            cb->pop_marker();
        }

        // Debug rendering
        if ( show_debug_rendering ) {
            debug_renderer.render( cb, &game_camera.camera, 0 );
        }

        cb->end_render_pass();

        cb->submit_barriers( { {game_rt, ResourceState::ShaderResource, 0, 1},
                             { game_depth_rt, ResourceState::ShaderResource, 0, 1 } }, {} );
        cb->pop_marker();

        // Swapchain rendering!
        idra::TextureHandle swapchain = gpu->get_current_swapchain_texture();

        // TODO: where should barriers be exposed ?
        cb->push_marker( "swapchain_pass" );

        cb->submit_barriers( { {swapchain, idra::ResourceState::RenderTarget, 0, 1} }, {} );
        cb->begin_pass( { swapchain }, { idra::LoadOperation::Clear }, { { 0, 0, 0, 1 } }, {}, idra::LoadOperation::DontCare, {} );

        cb->set_framebuffer_scissor();
        cb->set_framebuffer_viewport();

        // Imgui render
        g_imgui->render( *cb );

        cb->end_render_pass();

        cb->submit_barriers( { {swapchain, idra::ResourceState::Present, 0, 1} }, {} );
        cb->pop_marker();
        cb->pop_marker();

        gpu->enqueue_command_buffer( cb );
        gpu->present();
    }

    gpu->destroy_texture( game_rt );
    gpu->destroy_texture( game_depth_rt );

    destroy_resources( asset_manager, idra::AssetDestructionPhase::Shutdown );

    for ( u32 i = 0; i < render_systems.size; ++i ) {
        render_systems[ i ]->destroy_resources( asset_manager, idra::AssetDestructionPhase::Shutdown );
        render_systems[ i ]->shutdown();
    }
    render_systems.shutdown();

    // Shutdown systems and services
    ImGui::ApplicationLogShutdown();
    ImGui::FPSShutdown();

    idra::AssetManager::shutdown_system( asset_manager );

    g_imgui->shutdown();
    InputSystem::shutdown_system( input );
    window.shutdown();
    GpuDevice::shutdown_system( gpu );

    g_log->shutdown();
    g_memory->shutdown();

    tlsf_allocator.shutdown();
}

void DevGames2024Demo::setup_earth_atmosphere( AtmosphereParameters& info, f32 length_unit_in_meters ) {

    // Values shown here are the result of integration over wavelength power spectrum integrated with paricular function.
    // Refer to https://github.com/ebruneton/precomputed_atmospheric_scattering for details.

    // All units in kilometers
    const float EarthBottomRadius = 6360000.0f / length_unit_in_meters;
    const float EarthTopRadius = 6460000.0f / length_unit_in_meters;   // 100km atmosphere radius, less edge visible and it contain 99.99% of the atmosphere medium https://en.wikipedia.org/wiki/K%C3%A1rm%C3%A1n_line
    const float EarthRayleighScaleHeight = 8.0f;
    const float EarthMieScaleHeight = 1.2f;

    // Sun - This should not be part of the sky model...
    //info.solar_irradiance = { 1.474000f, 1.850400f, 1.911980f };
    info.solar_irradiance = { 1.0f, 1.0f, 1.0f };	// Using a normalise sun illuminance. This is to make sure the LUTs acts as a transfert factor to apply the runtime computed sun irradiance over.
    info.sun_angular_radius = 0.004675f;

    // Earth
    info.bottom_radius = EarthBottomRadius;
    info.top_radius = EarthTopRadius;
    info.ground_albedo = { 0.0f, 0.0f, 0.0f };

    // Move to more GPU friendly vec4 array.
    // Raleigh scattering
    //info.rayleigh_density.layers[ 0 ] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    info.rayleigh_density[ 0 ] = { 0, 0, 0, 0 };
    info.rayleigh_density[ 1 ] = { 0, 0, 1, -1.0f / EarthRayleighScaleHeight };
    info.rayleigh_density[ 2 ] = { 0, 0, -0.00142, -0.00142 };
    //info.rayleigh_density.layers[ 1 ] = { 0.0f, 1.0f, -1.0f / EarthRayleighScaleHeight, 0.0f, 0.0f };
    info.rayleigh_scattering = { 0.005802f, 0.013558f, 0.033100f };		// 1/km

    // Mie scattering
    //info.mie_density.layers[ 0 ] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    //info.mie_density.layers[ 1 ] = { 0.0f, 1.0f, -1.0f / EarthMieScaleHeight, 0.0f, 0.0f };
    info.mie_density[ 0 ] = { 0, 0, 0, 0 };
    info.mie_density[ 1 ] = { 0, 0.0f, 1.0f, -1.0f / EarthMieScaleHeight };
    info.mie_density[ 2 ] = { 0, 0, -0.00142, -0.00142 };
    info.mie_scattering = { 0.003996f, 0.003996f, 0.003996f };			// 1/km
    info.mie_extinction = { 0.004440f, 0.004440f, 0.004440f };			// 1/km
    info.mie_phase_function_g = 0.8f;

    // Ozone absorption
    //info.absorption_density.layers[ 0 ] = { 25.0f, 0.0f, 0.0f, 1.0f / 15.0f, -2.0f / 3.0f };
    info.absorption_density[ 0 ] = { 25.0f, 0.0f, 0.0f, 1.0f / 15.0f };
    info.absorption_density[ 1 ] = { -2.0f / 3.0f, 0, 0, 0 };
    info.absorption_density[ 2 ] = { -1.0f / 15.0f, 8.0f / 3.0f, -0.00142, -0.00142 };
    //info.absorption_density.layers[ 1 ] = { 0.0f, 0.0f, 0.0f, -1.0f / 15.0f, 8.0f / 3.0f };
    info.absorption_extinction = { 0.000650f, 0.001881f, 0.000085f };	// 1/km

    const double max_sun_zenith_angle = PI * 120.0 / 180.0; // (use_half_precision_ ? 102.0 : 120.0) / 180.0 * kPi;
    info.mu_s_min = ( float )cos( max_sun_zenith_angle );
}

} // namespace crb

// Main ///////////////////////////////////////////////////////////////////
int main( int argc, char** argv ) {

    idra::DevGames2024Demo demo;
    demo.main();

    return 0;
}
