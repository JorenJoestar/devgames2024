/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/string_view.hpp"

namespace idra {

namespace detail {
    constexpr bool utf8_trail_byte( char const in, char32_t& out ) noexcept {
        if ( in < 0x80 || 0xBF < in )
            return false;

        out = ( out << 6 ) | ( in & 0x3F );
        return true;
    }

    // Returns number of trailing bytes.
    // -1 on illegal header bytes.
    constexpr int utf8_header_byte( char const in, char32_t& out ) noexcept {
        if ( in < 0x80 ) {  // ASCII
            out = in;
            return 0;
        }
        if ( in < 0xC0 ) {  // not a header
            return -1;
        }
        if ( in < 0xE0 ) {
            out = in & 0x1F;
            return 1;
        }
        if ( in < 0xF0 ) {
            out = in & 0x0F;
            return 2;
        }
        if ( in < 0xF8 ) {
            out = in & 0x7;
            return 3;
        }
        return -1;
    }
}  // namespace detail

// UTF8 -> UTF16 converter ////////////////////////////////////////////////
constexpr ptrdiff_t utf8_to_utf16( StringView u8in,
                                   char16_t* u16out ) noexcept {
    ptrdiff_t outstr_size = 0;
    for ( u32 i = 0; i < u8in.constexpr_size(); ++i ) {
        char32_t code_point = 0;
        const i32 byte_cnt = detail::utf8_header_byte( u8in[i], code_point);

        if ( byte_cnt < 0 || byte_cnt > u8in.constexpr_size() )
            return false;

        for ( int j = 0; j < byte_cnt; ++j )
            if ( !detail::utf8_trail_byte( u8in[i + j], code_point) )
                return -1;

        if ( code_point < 0xFFFF ) {
            if ( u16out )
                *u16out++ = static_cast< char16_t >( code_point );
            ++outstr_size;
        } else {
            if ( u16out ) {
                code_point -= 0x10000;
                *u16out++ = static_cast< char16_t >( ( code_point >> 10 ) + 0xD800 );
                *u16out++ = static_cast< char16_t >( ( code_point & 0x3FF ) + 0xDC00 );
            }
            outstr_size += 2;
        }
    }
    return outstr_size;
}
	
} // namespace idra
