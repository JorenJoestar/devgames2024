/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/platform.hpp"

namespace idra {

    //
    // Color class that embeds color in a uint32.
    //
    struct Color {

        void                        set( float r, float g, float b, float a ) { abgr = uint8_t( r * 255.f ) | ( uint8_t( g * 255.f ) << 8 ) | ( uint8_t( b * 255.f ) << 16 ) | ( uint8_t( a * 255.f ) << 24 ); }

        f32                         r() const                               { return ( abgr & 0xff ) / 255.f; }
        f32                         g() const                               { return ( ( abgr >> 8 ) & 0xff ) / 255.f; }
        f32                         b() const                               { return ( ( abgr >> 16 ) & 0xff ) / 255.f; }
        f32                         a() const                               { return ( ( abgr >> 24 ) & 0xff ) / 255.f; }

        Color                       operator=( const u32 color )            { abgr = color; return *this; }

        static u32                  from_u8( u8 r, u8 g, u8 b, u8 a )       { return ( r | ( g << 8 ) | ( b << 16 ) | ( a << 24 ) ); }

        static u32                  get_distinct_color( u32 index );

        static const Color          red()           { return { 0xff0000ff }; }
        static const Color          green()         { return { 0xff00ff00 }; }
        static const Color          blue()          { return { 0xffff0000 }; }
        static const Color          yellow()        { return { 0xff00ffff }; }
        static const Color          black()         { return { 0xff000000 }; }
        static const Color          white()         { return { 0xffffffff }; }
        static const Color          transparent()   { return { 0x00000000 }; }

        u32                         abgr;

    }; // struct Color

} // namespace idra