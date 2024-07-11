/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */


#include "imgui/imgui_helpers.hpp"


namespace ImGui {

bool SliderUint( const char* label, u32* v, u32 v_min, u32 v_max, const char* format, ImGuiSliderFlags flags ) {
    return SliderScalar( label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags );
}

bool SliderUint2( const char* label, u32 v[ 2 ], u32 v_min, u32 v_max, const char* format, ImGuiSliderFlags flags ) {
    return SliderScalarN( label, ImGuiDataType_U32, v, 2, &v_min, &v_max, format, flags );
}

bool SliderUint3( const char* label, u32 v[ 3 ], u32 v_min, u32 v_max, const char* format, ImGuiSliderFlags flags ) {
    return SliderScalarN( label, ImGuiDataType_U32, v, 3, &v_min, &v_max, format, flags );
}

bool SliderUint4( const char* label, u32 v[ 4 ], u32 v_min, u32 v_max, const char* format, ImGuiSliderFlags flags ) {
    return SliderScalarN( label, ImGuiDataType_U32, v, 4, &v_min, &v_max, format, flags );
}

void Image( idra::TextureHandle& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col ) {
    Image( &texture.index, size, uv0, uv1, tint_col, border_col );
}

} // namespace ImGui
