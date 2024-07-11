/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#if !defined(_MSC_VER)
#include <signal.h>
#endif

// Macros ////////////////////////////////////////////////////////////////

#define ArraySize(array)        ( sizeof(array)/sizeof((array)[0]) )


#if defined (_MSC_VER)
#define IDRA_INLINE                             inline
#define IDRA_FINLINE                            __forceinline
#define IDRA_DEBUG_BREAK                        __debugbreak();
#define IDRA_DISABLE_WARNING(warning_number)    __pragma( warning( disable : warning_number ) )
#define IDRA_CONCAT_OPERATOR(x, y)              x##y
#else
#define IDRA_INLINE                             inline
#define IDRA_FINLINE                            always_inline
#define IDRA_DEBUG_BREAK                        raise(SIGTRAP);
#define IDRA_CONCAT_OPERATOR(x, y)              x y
#endif // MSVC

#define IDRA_STRINGIZE( L )                     #L 
#define IDRA_MAKESTRING( L )                    IDRA_STRINGIZE( L )
#define IDRA_CONCAT(x, y)                       IDRA_CONCAT_OPERATOR(x, y)
#define IDRA_LINE_STRING                        IDRA_MAKESTRING( __LINE__ ) 
#define IDRA_FILELINE(MESSAGE)                  __FILE__ "(" IDRA_LINE_STRING ") : " MESSAGE

// Unique names
#define IDRA_UNIQUE_SUFFIX(PARAM)               IDRA_CONCAT(PARAM, __LINE__ )


// Native types typedefs /////////////////////////////////////////////////
typedef uint8_t                 u8;
typedef uint16_t                u16;
typedef uint32_t                u32;
typedef uint64_t                u64;

typedef int8_t                  i8;
typedef int16_t                 i16;
typedef int32_t                 i32;
typedef int64_t                 i64;

typedef float                   f32;
typedef double                  f64;

typedef size_t                  sizet;

typedef const char*             cstring;
typedef uintptr_t               uintptr;
typedef intptr_t                intptr;

static const u64                u64_max = UINT64_MAX;
static const i64                i64_max = INT64_MAX;
static const u32                u32_max = UINT32_MAX;
static const i32                i32_max = INT32_MAX;
static const u16                u16_max = UINT16_MAX;
static const i16                i16_max = INT16_MAX;
static const u8                  u8_max = UINT8_MAX;
static const i8                  i8_max = INT8_MAX;

// Helper macros for sizes
#define ikilo(size)                 (size * 1024)
#define imega(size)                 (size * 1024 * 1024)
#define igiga(size)                 (size * 1024 * 1024 * 1024)