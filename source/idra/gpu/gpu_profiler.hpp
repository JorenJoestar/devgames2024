#pragma once

#include "kernel/memory.hpp"
#include "gpu/gpu_device.hpp"

namespace idra {


//
// A single timestamp query, containing indices for the pool, resolved time, name and color.
struct GPUTimeQuery {

    f64                             elapsed_ms;

    u16                             start_query_index;  // Used to write timestamp in the query pool
    u16                             end_query_index;    // Used to write timestamp in the query pool

    u16                             parent_index;
    u16                             depth;

    u32                             color;
    u32                             frame_index;

    StringView                      name;
}; // struct GPUTimeQuery

//
// Query tree used mainly per thread-frame to retrieve time data.
struct GpuTimeQueryTree {

    void                            reset();
    void                            set_queries( GPUTimeQuery* time_queries, u32 count );

    GPUTimeQuery*                   push( StringView name );
    GPUTimeQuery*                   pop();

    Span<GPUTimeQuery>              time_queries; // Allocated externally
    
    u16                             current_time_query   = 0;
    u16                             allocated_time_query = 0;
    u16                             depth                = 0;
    u16                             max_queries          = 0;

}; // struct GpuTimeQueryTree

//
//
struct GpuPipelineStatistics {
    enum Statistics : u8 {
        VerticesCount,
        PrimitiveCount,
        VertexShaderInvocations,
        ClippingInvocations,
        ClippingPrimitives,
        FragmentShaderInvocations,
        ComputeShaderInvocations,
        Count
    };

    void                            reset();

    u64                             statistics[ Count ];
};

// GpuVisualProfiler //////////////////////////////////////////////////////

//
// Collect per frame queries from GpuProfiler and create a visual representation.
struct GpuVisualProfiler {

    void                        init( Allocator* allocator, u32 max_frames, u32 max_queries_per_frame );
    void                        shutdown();

    void                        update( GpuDevice& gpu );

    void                        imgui_draw();

    Allocator*                  allocator;
    GPUTimeQuery*               timestamps;     // Per frame timestamps collected from the profiler.
    u16*                        per_frame_active;
    GpuPipelineStatistics*      pipeline_statistics;    // Per frame collected pipeline statistics.

    u32                         max_frames;
    u32                         max_queries_per_frame;
    u32                         current_frame;
    u32                         max_visible_depth = 2;

    f32                         max_time;
    f32                         min_time;
    f32                         average_time;

    f32                         max_duration;
    bool                        paused;

}; // struct GPUProfiler

} // namespace idra