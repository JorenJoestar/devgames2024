/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#if !defined(IDRA_SC_EXPORT)
#define IDRA_SC_EXPORT /* NOTHING */

#if defined(WIN32) || defined(WIN64)
#undef IDRA_SC_EXPORT
#if defined(asset_compiler_EXPORTS)
#define IDRA_SC_EXPORT __declspec(dllexport)
#else
#define IDRA_SC_EXPORT __declspec(dllimport)
#endif // defined(asset_compiler_EXPORTS)
#endif // defined(WIN32) || defined(WIN64)

#if defined(__GNUC__) || defined(__APPLE__) || defined(LINUX)
#if defined(asset_compiler_EXPORTS)
#undef IDRA_SC_EXPORT
#define IDRA_SC_EXPORT __attribute__((visibility("default")))
#endif // defined(asset_compiler_EXPORTS)
#endif // defined(__GNUC__) || defined(__APPLE__) || defined(LINUX)

#endif // !defined(IDRA_SC_EXPORT)

#include "kernel/string_view.hpp"
#include "kernel/blob.hpp"

#include "graphics/graphics_blueprints.hpp"

namespace idra {

IDRA_SC_EXPORT void asset_compiler_main( StringView source_folder, StringView destination_folder );

} // namespace idra
