#include "gpu_profiler.hpp"

#include "kernel/hash_map.hpp"
#include "kernel/numerics.hpp"
#include "kernel/color.hpp"

#include "gpu/gpu_device.hpp"
#include "gpu/idra_imgui.hpp"

#include "imgui/imgui_helpers.hpp"

#include <cmath>
#include <stdio.h>

namespace idra {

// GPU task names to colors
idra::FlatHashMap<u64, u32>   name_to_color;

static u32      initial_frames_paused = 15;

void GpuVisualProfiler::init( Allocator* allocator_, u32 max_frames_, u32 max_queries_per_frame_ ) {

    allocator = allocator_;
    max_frames = max_frames_;
    max_queries_per_frame = max_queries_per_frame_;
    timestamps = ( GPUTimeQuery* )ialloc( sizeof( GPUTimeQuery ) * max_frames * max_queries_per_frame, allocator );
    per_frame_active = ( u16* )ialloc( sizeof( u16 ) * max_frames, allocator );

    max_duration = 16.666f;
    current_frame = 0;
    min_time = max_time = average_time = 0.f;
    paused = false;
    pipeline_statistics = nullptr;

    memset( per_frame_active, 0, sizeof(u16) * max_frames );

    name_to_color.init( allocator, 16 );
    name_to_color.set_default_value( u32_max );
}

void GpuVisualProfiler::shutdown() {

    name_to_color.shutdown();

    ifree( timestamps, allocator );
    ifree( per_frame_active, allocator );
}

static f32 s_framebuffer_pixel_count = 0.f;

u32 resolve_timestamps( GpuDevice* gpu, u32 current_frame, GPUTimeQuery* timestamps_to_fill ) {
    // For each pool
    u32 copied_timestamps = 0;

    Span<CommandBuffer> span = gpu->command_buffer_manager->get_command_buffer_span( current_frame );

    for ( u32 q = 0; q < span.size; ++q ) {

        CommandBuffer* command_buffer = &span[ q ];
        GpuTimeQueryTree* time_query_tree = &command_buffer->time_query_tree;
        const u32 query_offset = ( current_frame + q * k_max_frames ) * gpu->command_buffer_manager->queries_per_pool;
        
        if ( time_query_tree->allocated_time_query ) {
            mem_copy( timestamps_to_fill + copied_timestamps, time_query_tree->time_queries.data, sizeof( GPUTimeQuery ) * time_query_tree->allocated_time_query );
            copied_timestamps += time_query_tree->allocated_time_query;
        }
    }

    return copied_timestamps;
}

void GpuVisualProfiler::update( GpuDevice& gpu ) {

    // TODO:
    //gpu.set_gpu_timestamps_enable( !paused );

    if ( initial_frames_paused ) {
        --initial_frames_paused;
        return;
    }

    if ( paused/* && !gpu.resized*/ )
        return;

    // Collect timestamps
    //u32 active_timestamps = gpu.copy_gpu_timestamps( &timestamps[ max_queries_per_frame * current_frame ] );
    u32 active_timestamps = resolve_timestamps( &gpu, gpu.previous_frame, &timestamps[ max_queries_per_frame * current_frame ] );
    per_frame_active[ current_frame ] = ( u16 )active_timestamps;

    // Collect pipeline statistics
    pipeline_statistics = nullptr;// &gpu.gpu_time_queries_manager->frame_pipeline_statistics;

    s_framebuffer_pixel_count = gpu.swapchain_width * gpu.swapchain_height * 1.f;

    // Get colors
    for ( u32 i = 0; i < active_timestamps; ++i ) {
        GPUTimeQuery& timestamp = timestamps[ max_queries_per_frame * current_frame + i ];

        const u64 hashed_name = idra::hash_calculate( timestamp.name );
        u32 color_index = name_to_color.get( hashed_name );
        // No entry found, add new color
        if ( color_index == u32_max ) {

            color_index = ( u32 )name_to_color.size;
            name_to_color.insert( hashed_name, color_index );
        }

        timestamp.color = idra::Color::get_distinct_color( color_index );
    }

    current_frame = ( current_frame + 1 ) % max_frames;

    // Reset Min/Max/Average after few frames
    if ( current_frame == 0 ) {
        max_time = -FLT_MAX;
        min_time = FLT_MAX;
        average_time = 0.f;
    }
}

void GpuVisualProfiler::imgui_draw() {
    if ( initial_frames_paused ) {
        return;
    }

    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        f32 widget_height = canvas_size.y - 100;

        f32 legend_width = 250;
        f32 graph_width = fabsf( canvas_size.x - legend_width );
        u32 rect_width = ceilu32( graph_width / max_frames );
        i32 rect_x = ceili32( graph_width - rect_width );

        f64 new_average = 0;

        ImGuiIO& io = ImGui::GetIO();

        static char buf[ 128 ];

        const ImVec2 mouse_pos = io.MousePos;

        i32 selected_frame = -1;

        // Draw time reference lines
        sprintf( buf, "%3.4fms", max_duration );
        draw_list->AddText( { cursor_pos.x, cursor_pos.y }, 0xff0000ff, buf );
        draw_list->AddLine( { cursor_pos.x + rect_width, cursor_pos.y }, { cursor_pos.x + graph_width, cursor_pos.y }, 0xff0000ff );

        sprintf( buf, "%3.4fms", max_duration / 2.f );
        draw_list->AddText( { cursor_pos.x, cursor_pos.y + widget_height / 2.f }, 0xff00ffff, buf );
        draw_list->AddLine( { cursor_pos.x + rect_width, cursor_pos.y + widget_height / 2.f }, { cursor_pos.x + graph_width, cursor_pos.y + widget_height / 2.f }, 0xff00ffff );

        // Draw Graph
        for ( u32 i = 0; i < max_frames; ++i ) {
            u32 frame_index = ( current_frame - 1 - i ) % max_frames;

            f32 frame_x = cursor_pos.x + rect_x;
            GPUTimeQuery* frame_timestamps = &timestamps[ frame_index * max_queries_per_frame ];
            f32 frame_time = ( f32 )frame_timestamps[ 0 ].elapsed_ms;
            // Clamp values to not destroy the frame data
            frame_time = idra::clamp( frame_time, 0.00001f, 1000.f );
            // Update timings
            new_average += frame_time;
            min_time = idra::min( min_time, frame_time );
            max_time = idra::max( max_time, frame_time );

            f32 rect_height = frame_time / max_duration * widget_height;
            //drawList->AddRectFilled( { frame_x, cursor_pos.y + rect_height }, { frame_x + rect_width, cursor_pos.y }, 0xffffffff );

            f32 current_height = cursor_pos.y;

            // Draw timestamps from the bottom
            for ( u32 j = 0; j < per_frame_active[ frame_index ]; ++j ) {
                const GPUTimeQuery& timestamp = frame_timestamps[ j ];

                // Draw only depth 1 timestamps, hierarchically under frame marker.
                if ( timestamp.depth != 1 ) {
                    continue;
                }

                // Margin used to identify better each column.
                static constexpr u32 width_margin = 2;

                rect_height = ( f32 )timestamp.elapsed_ms / max_duration * widget_height;
                const ImVec2 rect_min { frame_x + width_margin, current_height + widget_height - rect_height };
                const ImVec2 rect_max { frame_x + width_margin + rect_width - width_margin, current_height + widget_height };
                draw_list->AddRectFilled( rect_min, rect_max, timestamp.color );

                current_height -= rect_height;
            }

            if ( mouse_pos.x >= frame_x && mouse_pos.x < frame_x + rect_width &&
                mouse_pos.y >= cursor_pos.y && mouse_pos.y < cursor_pos.y + widget_height ) {
                draw_list->AddRectFilled( { frame_x, cursor_pos.y + widget_height },
                    { frame_x + rect_width, cursor_pos.y }, 0x0fffffff );

                ImGui::SetTooltip( "(%u): %f", frame_index, frame_time );

                selected_frame = frame_index;
            }

            draw_list->AddLine( { frame_x, cursor_pos.y + widget_height }, { frame_x, cursor_pos.y }, 0x0fffffff );

            // Debug only
            /*static char buf[ 32 ];
            sprintf( buf, "%u", frame_index );
            draw_list->AddText( { frame_x, cursor_pos.y + widget_height - 64 }, 0xffffffff, buf );

            sprintf( buf, "%u", frame_timestamps[0].frame_index );
            drawList->AddText( { frame_x, cursor_pos.y + widget_height - 32 }, 0xffffffff, buf );*/

            rect_x -= rect_width;
        }

        average_time = ( f32 )new_average / max_frames;

        // Draw legend
        ImGui::SetCursorPosX( cursor_pos.x + graph_width );
        // Default to last frame if nothing is selected.
        selected_frame = selected_frame == -1 ? ( current_frame - 1 ) % max_frames : selected_frame;
        if ( selected_frame >= 0 ) {
            GPUTimeQuery* frame_timestamps = &timestamps[ selected_frame * max_queries_per_frame ];

            f32 x = cursor_pos.x + graph_width + 8;
            f32 y = cursor_pos.y + widget_height - 14;

            for ( u32 j = 0; j < per_frame_active[ selected_frame ]; ++j ) {
                const GPUTimeQuery& timestamp = frame_timestamps[ j ];

                // Skip inner timestamps
                if ( timestamp.depth > max_visible_depth ) {
                    continue;
                }

                const f32 timestamp_x = x + timestamp.depth * 4;

                // Draw root (frame) on top
                if ( timestamp.depth == 0 ) {
                    draw_list->AddRectFilled( { timestamp_x, cursor_pos.y + 4 },
                                              { timestamp_x + 8, cursor_pos.y + 12 }, timestamp.color );

                    sprintf( buf, "%2.3fms %d %s", timestamp.elapsed_ms, timestamp.depth, timestamp.name.data );
                    draw_list->AddText( { timestamp_x + 20, cursor_pos.y }, 0xffffffff, buf );
                }
                else {
                    // Draw all other timestamps starting from bottom
                    draw_list->AddRectFilled( { timestamp_x, y + 4 },
                                              { timestamp_x + 8, y + 12 }, timestamp.color );

                    sprintf( buf, "%2.3fms %d %s", timestamp.elapsed_ms, timestamp.depth, timestamp.name.data );
                    draw_list->AddText( { timestamp_x + 20, y }, 0xffffffff, buf );

                    y -= 14;
                }
            }
        }

        ImGui::Dummy( { canvas_size.x, widget_height } );
    }

    ImGui::SetNextItemWidth( 100.f );
    ImGui::LabelText( "", "Max %3.4fms", max_time );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 100.f );
    ImGui::LabelText( "", "Min %3.4fms", min_time );
    ImGui::SameLine();
    ImGui::LabelText( "", "Ave %3.4fms", average_time );

    ImGui::Separator();
    ImGui::Checkbox( "Pause", &paused );

    static const char* items[] = { "200ms", "100ms", "66ms", "33ms", "16ms", "8ms", "4ms" };
    static const float max_durations[] = { 200.f, 100.f, 66.f, 33.f, 16.f, 8.f, 4.f };

    static int max_duration_index = 4;
    if ( ImGui::Combo( "Graph Max", &max_duration_index, items, IM_ARRAYSIZE( items ) ) ) {
        max_duration = max_durations[ max_duration_index ];
    }

    ImGui::SliderUint( "Max Depth", &max_visible_depth, 1, 4 );

    ImGui::Separator();
    static const char* stat_unit_names[] = { "Normal", "Kilo", "Mega" };
    static const char* stat_units[] = { "", "K", "M" };
    static const f32 stat_unit_multipliers[] = { 1.0f, 1000.f, 1000000.f };

    static int stat_unit_index = 1;
    const f32 stat_unit_multiplier = stat_unit_multipliers[ stat_unit_index ];
    cstring stat_unit_name = stat_units[ stat_unit_index ];
    if ( pipeline_statistics ) {
        f32 stat_values[ GpuPipelineStatistics::Count ];
        for ( u32 i = 0; i < GpuPipelineStatistics::Count; ++i ) {
            stat_values[ i ] = pipeline_statistics->statistics[ i ] / stat_unit_multiplier;
        }

        ImGui::Text( "Vertices %0.2f%s, Primitives %0.2f%s", stat_values[ GpuPipelineStatistics::VerticesCount ], stat_unit_name,
                     stat_values[ GpuPipelineStatistics::PrimitiveCount ], stat_unit_name );

        ImGui::Text( "Clipping: Invocations %0.2f%s, Visible Primitives %0.2f%s, Visible Perc %3.1f", stat_values[ GpuPipelineStatistics::ClippingInvocations ], stat_unit_name,
                     stat_values[ GpuPipelineStatistics::ClippingPrimitives ], stat_unit_name,
                     stat_values[ GpuPipelineStatistics::ClippingPrimitives ] / stat_values[ GpuPipelineStatistics::ClippingInvocations ] * 100.0f, stat_unit_name );

        ImGui::Text( "Invocations: Vertex Shaders %0.2f%s, Fragment Shaders %0.2f%s, Compute Shaders %0.2f%s", stat_values[ GpuPipelineStatistics::VertexShaderInvocations ], stat_unit_name,
                     stat_values[ GpuPipelineStatistics::FragmentShaderInvocations ], stat_unit_name, stat_values[ GpuPipelineStatistics::ComputeShaderInvocations ], stat_unit_name );

        ImGui::Text( "Invocations divided by number of full screen quad pixels." );
        ImGui::Text( "Vertex %0.2f, Fragment %0.2f, Compute %0.2f", stat_values[ GpuPipelineStatistics::VertexShaderInvocations ] * stat_unit_multiplier / s_framebuffer_pixel_count,
                     stat_values[ GpuPipelineStatistics::FragmentShaderInvocations ] * stat_unit_multiplier / s_framebuffer_pixel_count,
                     stat_values[ GpuPipelineStatistics::ComputeShaderInvocations ] * stat_unit_multiplier / s_framebuffer_pixel_count );

    }

    ImGui::Combo( "Stat Units", &stat_unit_index, stat_unit_names, IM_ARRAYSIZE( stat_unit_names ) );
}

// GpuTimeQueryTree ///////////////////////////////////////////////////////
void GpuTimeQueryTree::reset() {
    current_time_query = 0;
    allocated_time_query = 0;
    depth = 0;
}

void GpuTimeQueryTree::set_queries( GPUTimeQuery* time_queries_, u32 count ) {
    //time_queries.set( time_queries_, count );
    time_queries = Span<GPUTimeQuery>( time_queries_, count );
    max_queries = count;

    reset();
}

GPUTimeQuery* GpuTimeQueryTree::push( StringView name ) {

    iassert( allocated_time_query < max_queries );

    GPUTimeQuery& time_query = time_queries[ allocated_time_query ];
    time_query.start_query_index = allocated_time_query * 2;
    time_query.end_query_index = time_query.start_query_index + 1;
    time_query.depth = depth++;
    time_query.name = name;
    time_query.parent_index = current_time_query;

    current_time_query = allocated_time_query;
    ++allocated_time_query;

    return &time_query;
}

GPUTimeQuery* GpuTimeQueryTree::pop() {
    GPUTimeQuery& time_query = time_queries[ current_time_query ];
    current_time_query = time_query.parent_index;

    depth--;

    return &time_query;
}

// GpuPipelineStatistics //////////////////////////////////////////////////
void GpuPipelineStatistics::reset() {
    for ( u32 i = 0; i < Count; ++i ) {
        statistics[ i ] = 0;
    }
}

} // namespace idra
