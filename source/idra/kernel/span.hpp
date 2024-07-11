/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include <initializer_list>

#include "kernel/platform.hpp"

namespace idra {

// https://threadreaderapp.com/thread/1535253315654762501.html

template <typename Type>
struct Span {

    constexpr               Span()                                      : data( nullptr ), size( 0 ) {}
    constexpr               Span( Type* data, size_t size )             : data( data ), size( size ) {}
    // Note: Span should have const Type to compile.
    constexpr               Span( std::initializer_list<Type> list )    : data( list.begin() ), size( list.size() ) {}
    //template<sizet N>       Span(Type(&c_array)[N])                     : data(c_array), size(ArraySize(c_array)) {}

    constexpr const Type*   begin() const                               { return &data[ 0 ]; }
    constexpr Type*         begin()                                     { return &data[ 0 ]; }

    constexpr const Type*   end() const                                 { return &data[ size ]; }
    constexpr Type*         end()                                       { return &data[ size ]; }

    constexpr Type&         operator[]( const sizet index )             { return data[ index ]; }
    constexpr const Type&   operator[]( const sizet index ) const       { return data[ index ]; }


    constexpr sizet         constexpr_size()                            { return size; }

    Type*                   data                = nullptr;
    size_t                  size                = 0;

}; // struct Span

} // namespace idra
