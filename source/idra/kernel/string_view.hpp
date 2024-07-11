/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "span.hpp"

#include <string.h>

namespace idra {

struct StringView : public Span<const char> {

    constexpr StringView() : Span<const char>() {}
    StringView( cstring c_string ) : Span<const char>( c_string, strlen( c_string ) ) {}
    constexpr StringView( cstring data, size_t size ) : Span<const char>( data, size ) {}

}; // struct StringView

} // namespace idra
