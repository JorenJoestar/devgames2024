/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "platform.hpp"

namespace idra {

struct TimeTick {

    long long counter;

}; // struct TimeTick

//
//
struct TimeService {

    void            init();
    void            shutdown();

    TimeTick        now();
    TimeTick        delta( const TimeTick& a, const TimeTick& b );

    f64             convert_microseconds( const TimeTick& time );
    f64             convert_milliseconds( const TimeTick& time );
    f64             convert_seconds( const TimeTick& time );

}; // struct TimeService

extern TimeService* g_time;

} // namespace idra