/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "gpu/gpu_resources.hpp"

#include "external/imgui/imgui.h"

// Extend ImGui namespace with helpers
namespace ImGui {

bool SliderUint( const char* label, u32* v, u32 v_min, u32 v_max, const char* format = "%d", ImGuiSliderFlags flags = 0 );
bool SliderUint2( const char* label, u32 v[ 2 ], u32 v_min, u32 v_max, const char* format = "%d", ImGuiSliderFlags flags = 0 );
bool SliderUint3( const char* label, u32 v[ 3 ], u32 v_min, u32 v_max, const char* format = "%d", ImGuiSliderFlags flags = 0 );
bool SliderUint4( const char* label, u32 v[ 4 ], u32 v_min, u32 v_max, const char* format = "%d", ImGuiSliderFlags flags = 0 );

// Wrapper to use TextureHandle
void Image( idra::TextureHandle& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2( 0, 0 ), const ImVec2& uv1 = ImVec2( 1, 1 ), const ImVec4& tint_col = ImVec4( 1, 1, 1, 1 ), const ImVec4& border_col = ImVec4( 0, 0, 0, 0 ) );

} // namespace ImGui
