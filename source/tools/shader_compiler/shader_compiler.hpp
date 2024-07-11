/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#if !defined(IDRA_SC_EXPORT)
#define IDRA_SC_EXPORT /* NOTHING */

#if defined(WIN32) || defined(WIN64)
#undef IDRA_SC_EXPORT
#if defined(shader_compiler_EXPORTS)
#define IDRA_SC_EXPORT __declspec(dllexport)
#else
#define IDRA_SC_EXPORT __declspec(dllimport)
#endif // defined(shader_compiler_EXPORTS)
#endif // defined(WIN32) || defined(WIN64)

#if defined(__GNUC__) || defined(__APPLE__) || defined(LINUX)
#if defined(shader_compiler_EXPORTS)
#undef IDRA_SC_EXPORT
#define IDRA_SC_EXPORT __attribute__((visibility("default")))
#endif // defined(shader_compiler_EXPORTS)
#endif // defined(__GNUC__) || defined(__APPLE__) || defined(LINUX)

#endif // !defined(IDRA_SC_EXPORT)

#include "kernel/string_view.hpp"
#include "kernel/memory.hpp"
#include "kernel/log.hpp"

#include "gpu/gpu_enums.hpp"

// TODO: glslang uses vectors to store the result of SpirV compilation
#include <vector>

namespace idra {

struct ShaderCompilationInfo {
    Span<const StringView> defines;
    Span<const StringView> include_paths;
    StringView source_path;
    ShaderStage::Enum stage;
};

// Per process (not needed per thread) init/shutdown
IDRA_SC_EXPORT void shader_compiler_init( StringView shader_folder_path );
IDRA_SC_EXPORT void shader_compiler_shutdown();

IDRA_SC_EXPORT void shader_compiler_add_log_callback( PrintCallback callback );
IDRA_SC_EXPORT void shader_compiler_remove_log_callback( PrintCallback callback );

IDRA_SC_EXPORT void shader_compiler_compile( StringView source_code, ShaderStage::Enum stage, std::vector<unsigned int>& spirv );
IDRA_SC_EXPORT void shader_compiler_compile_from_file( const ShaderCompilationInfo& creation, std::vector<unsigned int>& spirv );

} // namespace idra
