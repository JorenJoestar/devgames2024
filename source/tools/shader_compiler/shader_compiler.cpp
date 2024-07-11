/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "shader_compiler.hpp"
#include <stdio.h>

#include "kernel/allocator.hpp"
#include "kernel/log.hpp"
#include "kernel/assert.hpp"
#include "kernel/file.hpp"
#include "kernel/memory.hpp"
#include "kernel/string.hpp"
#include "kernel/hash_map.hpp"
#include "kernel/lexer.hpp"
#include "kernel/numerics.hpp"
#include "kernel/time.hpp"

// glsl
#include "glslang/Public/ShaderLang.h"
#include "glslang/SPIRV/GlslangToSpv.h"

#include <string>

#include <vector>

namespace idra {

// Needs this to remove the dependency in cmake
// https://vulkan.lunarg.com/issue/view/656df8aa5df1125b58afb491

static const char* g_vertex_shader_code_vulkan = {
    "#version 450\n"
    "layout( location = 0 ) in vec2 Position;\n"
    "layout( std140, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
    "void main() {\n"
    "    gl_Position = vec4( Position.xy,0,1 );\n"
    "}\n"
};


const TBuiltInResource DEFAULT_BUILT_IN_RESOURCE_LIMIT = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* maxMeshOutputVerticesEXT;*/256,
    /* maxMeshOutputPrimitivesEXT;*/512,
    /* maxMeshWorkGroupSizeX_EXT;*/32,
    /* maxMeshWorkGroupSizeY_EXT;*/1,
    /* maxMeshWorkGroupSizeZ_EXT;*/32,
    /* maxTaskWorkGroupSizeX_EXT;*/1,
    /* maxTaskWorkGroupSizeY_EXT;*/1,
    /* maxTaskWorkGroupSizeZ_EXT;*/1,
    /* maxMeshViewCountEXT;*/1,
    /* .maxDualSourceDrawBuffersEXT = */1,
    /* .limits = */
                       {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
} };

// To check some examples
// https://github.com/Goutch/HellbenderEngine/blob/master/CMakeLists.txt

// Forward declarations ///////////////////////////////////////////////////

static EShLanguage  shader_stage_to_sh_language( ShaderStage::Enum stage );

static void         dump_shader_code( StringBuffer& temp_string_buffer, 
                                      cstring code, cstring name );

static u64          shader_concatenate( StringView path, StringBuffer& shader_code );
static void         log_lines_around_error( cstring code, cstring shader_code );

static char s_shader_folder_path[ 512 ];
MallocAllocator s_mallocator;

void shader_compiler_init( StringView shader_folder_path ) {
    glslang::InitializeProcess();

    // Cache shader folder path
    strcpy( s_shader_folder_path, shader_folder_path.data );

    g_log->init( &s_mallocator );
    g_time->init();
}

void shader_compiler_shutdown() {
    glslang::FinalizeProcess();

    g_log->shutdown();
    g_time->shutdown();
}

void shader_compiler_add_log_callback( PrintCallback callback ) {
    g_log->add_callback( callback );
}

void shader_compiler_remove_log_callback( PrintCallback callback ) {
    g_log->remove_callback( callback );
}

void shader_compiler_compile( StringView source_code, ShaderStage::Enum stage, std::vector<unsigned int>& spirv ) {
    ilog( "Shader compiler compiling...\n\n" );

    const EShLanguage sh_language = shader_stage_to_sh_language( stage );
    glslang::TShader shader( sh_language );
    glslang::TProgram program;

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = ( EShMessages )( EShMsgSpvRules | EShMsgVulkanRules );

    cstring shaderStrings[ 1 ];

    shaderStrings[ 0 ] = source_code.data;
    shader.setStrings( shaderStrings, 1 );

    shader.setEnvClient( glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_3 );
    shader.setEnvInput( glslang::EShSourceGlsl, sh_language, glslang::EShClientVulkan, 450 );
    shader.setEnvTarget( glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_3 );

    bool parsing_error = false;
    if ( !shader.parse( &DEFAULT_BUILT_IN_RESOURCE_LIMIT, 450, false, messages ) ) {

        ilog( shader.getInfoLog() );
        ilog( shader.getInfoDebugLog() );

        log_lines_around_error( shader.getInfoLog(), source_code.data );

        parsing_error = true;
    }

    program.addShader( &shader );

    bool linking_error = false;
    // Program-level processing...
    if ( !program.link( messages ) ) {
        ilog( shader.getInfoLog() );
        ilog( shader.getInfoDebugLog() );

        log_lines_around_error( shader.getInfoLog(), source_code.data );

        linking_error = true;
    }

    if ( !( parsing_error || linking_error ) ) {
        glslang::GlslangToSpv( *program.getIntermediate( sh_language ), spirv );
    }
}


static u64 shader_concatenate( StringView path, StringBuffer& shader_code ) {
    
    MallocAllocator mallocator;

    Span<char> buffer_span = file_read_allocate( path, &mallocator );
    if ( buffer_span.size == 0 ) {
        ilog_error( "Error opening file %s\n", path.data );
        return 0;
    }

    shader_code.append_m( buffer_span.data, buffer_span.size );

    u64 hashed_memory = hash_bytes( buffer_span.data, buffer_span.size );
    return hashed_memory;
}

void dump_shader_code( StringBuffer& temp_string_buffer, cstring code, cstring name ) {
    //ilog( "Error in creation of shader %s, stage %s. Writing shader:\n", name, to_stage_defines( stage ) );

    cstring current_code = code;
    u32 line_index = 1;
    while ( current_code ) {

        cstring end_of_line = current_code;
        if ( !end_of_line || *end_of_line == 0 ) {
            break;
        }
        while ( !is_end_of_line( *end_of_line ) ) {
            ++end_of_line;
        }
        if ( *end_of_line == '\r' ) {
            ++end_of_line;
        }
        if ( *end_of_line == '\n' ) {
            ++end_of_line;
        }

        temp_string_buffer.clear();
        StringView line = temp_string_buffer.append_use_substring( current_code, 0, ( end_of_line - current_code ) );
        ilog( "%u: %s", line_index++, line.data );

        current_code = end_of_line;
    }
}

void shader_compiler_compile_from_file( const ShaderCompilationInfo& creation, std::vector<unsigned int>& spirv ) {
    ilog( "Shader compiler compiling file %s!\n", creation.source_path.data );

    // Change directory to shader directory
    Directory current_directory;
    fs_directory_current( &current_directory );

    fs_directory_change( s_shader_folder_path );

    MallocAllocator mallocator;
    StringBuffer shader_code;
    shader_code.init( ikilo( 800 ), &mallocator );

    shader_code.append_f( "#version 460\n" );

    // Append defines
    for ( u32 i = 0; i < ( u32 )creation.defines.size; ++i ) {
        shader_code.append_f( "#define %s\n", creation.defines[ i ].data );
    }

    // Append include code
    for ( u32 i = 0; i < ( u32 )creation.include_paths.size; ++i ) {
        shader_concatenate( creation.include_paths[ i ], shader_code );
    }

    u64 shader_file_hash = shader_concatenate( creation.source_path, shader_code );
    // Add the null terminator at the end of the concatenated file.
    shader_code.data[ shader_code.current_size ] = 0;

    shader_compiler_compile( StringView(shader_code.data, shader_code.current_size), creation.stage, spirv );

    if ( spirv.size() == 0 ) {

        StringBuffer error_report;
        error_report.init( ikilo( 800 ), &mallocator );

        // TODO:
        //dump_shader_code( error_report, shader_code.data, creation.source_path.data );
    }
    else {
        ilog( "Compilation successful!\n" );
    }

    free( ( char* )shader_code.data );

    fs_directory_change( current_directory.path );
}


// ShaderStage to SHLanguage
EShLanguage shader_stage_to_sh_language( ShaderStage::Enum stage ) {
    switch ( stage ) {
        case ShaderStage::AnyHit:
            return EShLangAnyHit;

        case ShaderStage::Callable:
            return EShLangCallable;

        case ShaderStage::Closest:
            return EShLangClosestHit;

        case ShaderStage::Compute:
            return EShLangCompute;

        case ShaderStage::Fragment:
            return EShLangFragment;

        case ShaderStage::Intersect:
            return EShLangIntersect;

        case ShaderStage::Mesh:
            return EShLangMesh;

        case ShaderStage::Miss:
            return EShLangMiss;

        case ShaderStage::RayGen:
            return EShLangRayGen;

        case ShaderStage::Task:
            return EShLangTask;

        case ShaderStage::Vertex:
            return EShLangVertex;
    }

    iassert( false );
    return EShLangIntersect;
}


void log_lines_around_error( cstring code, cstring shader_code ) {
    const char* error_string = strstr( code, "ERROR" );
    // Error format is: ERROR: filename:(line)
    error_string = strstr( error_string, ":" );
    ++error_string;
    error_string = strstr( error_string, ":" );
    ++error_string;
    i32 error_line = atoi( error_string );

    const i32 k_output_error_lines = 16;
    // Write 5 lines before and 4 after the error line.
    i32 min_line = idra::max( 0, error_line - k_output_error_lines / 2 );
    // Search line
    char* shader_error_line = (char*)shader_code;
    char* string_buffer = ( char* )malloc( ikilo( 8 ) );

    cstring current_code = shader_code;
    u32 line_index = 1;
    
    Lexer lexer;
    lexer_init( &lexer, shader_error_line, nullptr );
    lexer_goto_line( &lexer, min_line );

    shader_error_line = lexer.position;

    lexer_next_line( &lexer );
    char* shader_next_error_line = lexer.position;

    int32_t shader_line_size = ( i32 )( shader_next_error_line - shader_error_line );
    // Output lines before the error line.
    // Need to limit the output to just one line at the time.
    for ( size_t i = 0; i < k_output_error_lines; i++ ) {

        if ( *shader_error_line == 0 ) {
            break;
        }

        memcpy( string_buffer, shader_error_line, shader_line_size );
        string_buffer[ shader_line_size ] = 0;

        ilog( "%u: %s%s", i + min_line + 1, ( i + min_line == error_line - 1 ) ? "ERROR LINE: " : "", string_buffer );

        // Advance one line
        shader_error_line = shader_next_error_line;
        lexer_next_line( &lexer );
        shader_next_error_line = lexer.position;

        shader_line_size = ( i32 )( shader_next_error_line - shader_error_line );
    }

    free( string_buffer );

    ilog( "Done\n" );
}


} // namespace idra