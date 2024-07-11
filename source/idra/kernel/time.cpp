/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "time.hpp"


#if defined(_MSC_VER)
#include <Windows.h>
#else
#include <time.h>
#endif // _MSC_VER

namespace idra {

#if defined(_MSC_VER)
// Cached frequency.
// From Microsoft Docs: (https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency)
// "The frequency of the performance counter is fixed at system boot and is consistent across all processors.
// Therefore, the frequency need only be queried upon application initialization, and the result can be cached."
static LARGE_INTEGER s_frequency;
#endif

static TimeService s_time_service;
extern TimeService* g_time = &s_time_service;

//
//
void TimeService::init() {
#if defined(_MSC_VER)
    // Cache this value - by Microsoft Docs it will not change during process lifetime.
    QueryPerformanceFrequency( &s_frequency );
#endif // _MSC_VER
}

//
//
void TimeService::shutdown() {
    // Nothing to do.
}

// Taken from the Rust code base: https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
// Computes (value*numer)/denom without overflow, as long as both
// (numer*denom) and the overall result fit into i64 (which is the case
// for our time conversions).
static i64 int64_mul_div( i64 value, i64 numer, i64 denom ) {
    const i64 q = value / denom;
    const i64 r = value % denom;
    // Decompose value as (value/denom*denom + value%denom),
    // substitute into (value*numer)/denom and simplify.
    // r < denom, so (denom*numer) is the upper bound of (r*numer)
    return q * numer + r * numer / denom;
}

//
//
TimeTick TimeService::now() {
#if defined(_MSC_VER)
    // Get current time
    LARGE_INTEGER time;
    QueryPerformanceCounter( &time );

    TimeTick time0{ .counter = time.QuadPart };
    return time0;

    // Convert to microseconds
    // const i64 microseconds_per_second = 1000000LL;
    //const i64 microseconds = int64_mul_div( time.QuadPart, 1000000LL, s_frequency.QuadPart );
#else
    timespec tp;
    clock_gettime( CLOCK_MONOTONIC, &tp );

    const long long now = tp.tv_sec * 1000000000 + tp.tv_nsec;
    return { now };

#endif // _MSC_VER
}

//
//
TimeTick TimeService::delta( const TimeTick& a, const TimeTick& b ) {
    return TimeTick{ .counter = a.counter - b.counter };
}

//
//
f64 TimeService::convert_microseconds( const TimeTick& time ) {
#if defined(_MSC_VER)
    const i64 microseconds = int64_mul_div( time.counter, 1000000LL, s_frequency.QuadPart );
    return ( double )microseconds;
#else
    return ( double )time.counter / 1000;
#endif // _MSC_VER
}

//
//
f64 TimeService::convert_milliseconds( const TimeTick& time ) {
#if defined(_MSC_VER)
    const i64 milliseconds = int64_mul_div( time.counter, 1000000LL, s_frequency.QuadPart );
    return ( double )milliseconds / 1000.0;
#else
    return ( double )time.counter / 1000000;
#endif // _MSC_VER
}

//
//
f64 TimeService::convert_seconds( const TimeTick& time ) {
#if defined(_MSC_VER)
    const i64 seconds = int64_mul_div( time.counter, 1000000LL, s_frequency.QuadPart );
    return ( double )seconds / 1000000.0;
#else
    return ( double )time.counter / 1000000000;
#endif // _MSC_VER
}

} // namespace idra