#include "graphics/debug_renderer.hpp"

#include "gpu/gpu_device.hpp"
#include "gpu/command_buffer.hpp"
#include "graphics/graphics_asset_loaders.hpp"

#include "kernel/camera.hpp"

namespace idra {

// DebugRenderer //////////////////////////////////////////////////////////

//
//
struct LineVertex {
    vec3s                           position;
    Color                           color;

    void                            set( vec3s position_, Color color_ ) { position = position_; color = color_; }
    void                            set( vec2s position_, Color color_ ) { position = { position_.x, position_.y, 0 }; color = color_; }
}; // struct LineVertex

//
//
struct LineVertex2D {
    vec3s                           position;
    u32                             color;

    void                            set( vec2s position_, Color color_ ) { position = { position_.x, position_.y, 0 }, color = color_.abgr; }
}; // struct LineVertex2D

//
//
struct DebugRendererGpuConstants {

    mat4s                   view_projection;

    f32                     resolution_x;
    f32                     resolution_y;
    f32                     padding[ 2 ];
};

static const u32            k_max_lines = 1024 * 1024;

static LineVertex*          s_line_buffer;
static LineVertex2D*        s_line_buffer_2d;

DebugRenderer::DebugRenderer( u32 view_count_, u32 max_lines_ ) {
    view_count = view_count_;
    max_lines = max_lines_;
}

void DebugRenderer::init( GpuDevice* gpu_device_, Allocator* resident_allocator ) {

    gpu_device = gpu_device_;
    // Check correct values
    iassert( max_lines > 0 && view_count > 0 );
    
    current_line_per_view.init( resident_allocator, view_count, view_count );
    current_line_2d_per_view.init( resident_allocator, view_count, view_count );

    for ( u32 i = 0; i < view_count; ++ i) {
        current_line_per_view[ i ] = 0;
        current_line_2d_per_view[ i ] = 0;
    }
}

void DebugRenderer::shutdown() {

    current_line_per_view.shutdown();
    current_line_2d_per_view.shutdown();
}

void DebugRenderer::create_resources( AssetManager* asset_manager, AssetCreationPhase::Enum phase ) {

    if ( phase == AssetCreationPhase::Startup ) {
        ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();

        draw_shader = shader_loader->compile_graphics( {}, {}, "debug_line_cpu.vert", "debug_line.frag", "debug_line_draw" );
        draw_2d_shader = shader_loader->compile_graphics( {}, {}, "debug_line_2d_cpu.vert", "debug_line.frag", "debug_line_draw_2d" );

        // Just use the dynamic constants
        debug_lines_layout = gpu_device->create_descriptor_set_layout( {
                .dynamic_buffer_bindings = { 0 },
                .debug_name = "debug_lines_layout" } );

        debug_lines_draw_set = gpu_device->create_descriptor_set( {
            .dynamic_buffer_bindings = {{0,sizeof( DebugRendererGpuConstants )}},
            .layout = debug_lines_layout,
            .debug_name = "debug_lines_draw_set" } );

        iassert( max_lines > 0 && view_count > 0 );

        // Create buffers
        lines_vb = gpu_device->create_buffer( {
                .type = BufferUsage::Vertex_mask, .usage = ResourceUsageType::Dynamic,
                .size = ( u32 )( sizeof( LineVertex ) * max_lines * view_count ), .persistent = 1, .device_only = 0, .initial_data = nullptr,
                .debug_name = "lines_vb" } );

        lines_vb_2d = gpu_device->create_buffer( {
            .type = BufferUsage::Vertex_mask, .usage = ResourceUsageType::Dynamic,
            .size = ( u32 )( sizeof( LineVertex2D ) * max_lines * view_count), .persistent = 1, .device_only = 0, .initial_data = nullptr,
            .debug_name = "lines_vb_2d" } );

        // Cache pointers
        Buffer* lines_buffer = gpu_device->buffers.get_cold( lines_vb );
        s_line_buffer = ( LineVertex* )lines_buffer->mapped_data;
        iassert( s_line_buffer );

        Buffer* lines_buffer_2d = gpu_device->buffers.get_cold( lines_vb_2d );
        s_line_buffer_2d = ( LineVertex2D* )lines_buffer_2d->mapped_data;
        iassert( s_line_buffer_2d );
    }

    // These are always created
    debug_lines_draw_pipeline = gpu_device->create_graphics_pipeline( {
        .rasterization = {},
        .depth_stencil = {.depth_comparison = ComparisonFunction::Always, .depth_enable = 1, .depth_write_enable = 0 },
        .blend_state = {.blend_states = { {.source_color = Blend::SrcAlpha,
                                          .destination_color = Blend::InvSrcAlpha,
                                          .color_operation = BlendOperation::Add,
                                           } } },
        .vertex_input = {.vertex_streams = { {.binding = 0, .stride = 32, .input_rate = VertexInputRate::PerInstance} },
                         .vertex_attributes = { { 0, 0, 0, VertexComponentFormat::Float3 },
                                                { 1, 0, 12, VertexComponentFormat::UByte4N },
                                                { 2, 0, 16, VertexComponentFormat::Float3 },
                                                { 3, 0, 28, VertexComponentFormat::UByte4N }}},
        .shader = draw_shader->shader,
        .descriptor_set_layouts = { debug_lines_layout },
        .viewport = {},
        .color_formats = { gpu_device->swapchain_format },
        .depth_format = TextureFormat::D32_FLOAT,
        .debug_name = "debug_lines_draw_pipeline" } );

    debug_lines_2d_draw_pipeline = gpu_device->create_graphics_pipeline( {
        .rasterization = {},
        .depth_stencil = {.depth_comparison = ComparisonFunction::Always, .depth_enable = 1, .depth_write_enable = 0 },
        .blend_state = {.blend_states = { {.source_color = Blend::SrcAlpha,
                                          .destination_color = Blend::InvSrcAlpha,
                                          .color_operation = BlendOperation::Add,
                                           } } },
        .vertex_input = {.vertex_streams = { {.binding = 0, .stride = 32, .input_rate = VertexInputRate::PerInstance} },
                         .vertex_attributes = { { 0, 0, 0, VertexComponentFormat::Float3 },
                                                { 1, 0, 12, VertexComponentFormat::UByte4N },
                                                { 2, 0, 16, VertexComponentFormat::Float3 },
                                                { 3, 0, 28, VertexComponentFormat::UByte4N }}},
        .shader = draw_2d_shader->shader,
        .descriptor_set_layouts = { debug_lines_layout },
        .viewport = {},
        .color_formats = { gpu_device->swapchain_format },
        .depth_format = TextureFormat::D32_FLOAT,
        .debug_name = "debug_lines_draw_pipeline" } );

}

void DebugRenderer::destroy_resources( AssetManager* asset_manager, AssetDestructionPhase::Enum phase ) {

    gpu_device->destroy_pipeline( debug_lines_draw_pipeline );
    gpu_device->destroy_pipeline( debug_lines_2d_draw_pipeline );

    if ( phase == AssetDestructionPhase::Reload ) {        
        return;
    }

    ShaderAssetLoader* shader_loader = asset_manager->get_loader<ShaderAssetLoader>();
    shader_loader->unload( draw_shader );
    shader_loader->unload( draw_2d_shader );

    gpu_device->destroy_buffer( lines_vb );
    gpu_device->destroy_buffer( lines_vb_2d );
    gpu_device->destroy_pipeline( debug_lines_draw_pipeline );
    gpu_device->destroy_pipeline( debug_lines_2d_draw_pipeline );
    gpu_device->destroy_descriptor_set_layout( debug_lines_layout );
    gpu_device->destroy_descriptor_set( debug_lines_draw_set );
}

void DebugRenderer::render( CommandBuffer* gpu_commands, Camera* camera, u32 phase ) {
    
    if ( phase >= view_count ) {
        ilog_warn( "DebugRenderer::render error: view count (%d) is bigger than actual views (%d)\n", phase, view_count );
        return;
    }

    const u32 current_line = current_line_per_view[ phase ];
    const u32 current_line_2d = current_line_2d_per_view[ phase ];

    u32 dynamic_constants_offset = 0;

    if ( current_line || current_line_2d ) {
        // Upload dynamic constants
        DebugRendererGpuConstants* gpu_constants = gpu_device->dynamic_buffer_allocate<DebugRendererGpuConstants>( &dynamic_constants_offset );
        if ( gpu_constants ) {

            gpu_constants->view_projection = camera->view_projection;
            gpu_constants->resolution_x = camera->viewport_width * 1.f;
            gpu_constants->resolution_y = camera->viewport_height * 1.f;
        }
    }

    if ( current_line ) {

        const u32 vertex_buffer_offset = phase * max_lines * sizeof( LineVertex );

        gpu_commands->bind_pipeline( debug_lines_draw_pipeline );
        gpu_commands->bind_vertex_buffer( lines_vb, 0, vertex_buffer_offset );
        gpu_commands->bind_descriptor_set( { debug_lines_draw_set }, { dynamic_constants_offset } );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        gpu_commands->draw( TopologyType::Triangle, 0, num_vertices, 0, current_line / 2 );

        current_line_per_view[ phase ] = 0;
    }

    if ( current_line_2d ) {

        const u32 vertex_buffer_offset = phase * max_lines * sizeof( LineVertex2D );

        gpu_commands->bind_pipeline( debug_lines_2d_draw_pipeline );
        gpu_commands->bind_vertex_buffer( lines_vb_2d, 0, vertex_buffer_offset );
        gpu_commands->bind_descriptor_set( { debug_lines_draw_set }, { dynamic_constants_offset } );
        // Draw using instancing and 6 vertices.
        const uint32_t num_vertices = 6;
        gpu_commands->draw( TopologyType::Triangle, 0, num_vertices, 0, current_line_2d / 2 );

        current_line_2d_per_view[ phase ] = 0;
    }
}

void DebugRenderer::line( const vec3s& from, const vec3s& to, Color color, u32 view_index ) {
    line( from, to, color, color, view_index );
}

void DebugRenderer::line_2d( const vec2s& from, const vec2s& to, Color color, u32 view_index ) {

    if ( view_index >= view_count ) {
        ilog_warn( "DebugRenderer error: view count (%d) is bigger than actual views (%d)\n", view_index, view_count );
        return;
    }

    if ( current_line_2d_per_view[ view_index ] >= k_max_lines ) {
        return;
    }

    const u32 current_line_2d = current_line_2d_per_view[ view_index ];
    const u32 line_write_offset = ( view_index * max_lines ) + current_line_2d;
    s_line_buffer_2d[ line_write_offset ].set( from, color );
    s_line_buffer_2d[ line_write_offset + 1 ].set( to, color );

    current_line_2d_per_view[ view_index ] += 2;
}

void DebugRenderer::line( const vec3s& from, const vec3s& to, Color color0, Color color1, u32 view_index ) {

    if ( view_index >= view_count ) {
        ilog_warn( "DebugRenderer error: view count (%d) is bigger than actual views (%d)\n", view_index, view_count );
        return;
    }

    if ( current_line_per_view[ view_index ] >= k_max_lines ) {
        return;
    }

    const u32 current_line = current_line_per_view[ view_index ];
    const u32 line_write_offset = ( view_index * max_lines ) + current_line;
    s_line_buffer[ line_write_offset ].set( from, color0 );
    s_line_buffer[ line_write_offset + 1 ].set( to, color1 );

    current_line_per_view[ view_index ] += 2;
}

void DebugRenderer::aabb( const vec3s& min, const vec3s max, Color color, u32 view_index ) {

    const f32 x0 = min.x;
    const f32 y0 = min.y;
    const f32 z0 = min.z;
    const f32 x1 = max.x;
    const f32 y1 = max.y;
    const f32 z1 = max.z;

    line( { x0, y0, z0 }, { x0, y1, z0 }, color, color, view_index );
    line( { x0, y1, z0 }, { x1, y1, z0 }, color, color, view_index );
    line( { x1, y1, z0 }, { x1, y0, z0 }, color, color, view_index );
    line( { x1, y0, z0 }, { x0, y0, z0 }, color, color, view_index );
    line( { x0, y0, z0 }, { x0, y0, z1 }, color, color, view_index );
    line( { x0, y1, z0 }, { x0, y1, z1 }, color, color, view_index );
    line( { x1, y1, z0 }, { x1, y1, z1 }, color, color, view_index );
    line( { x1, y0, z0 }, { x1, y0, z1 }, color, color, view_index );
    line( { x0, y0, z1 }, { x0, y1, z1 }, color, color, view_index );
    line( { x0, y1, z1 }, { x1, y1, z1 }, color, color, view_index );
    line( { x1, y1, z1 }, { x1, y0, z1 }, color, color, view_index );
    line( { x1, y0, z1 }, { x0, y0, z1 }, color, color, view_index );
}


} // namespace idra