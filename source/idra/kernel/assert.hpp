/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "log.hpp"

namespace idra {

#define iassert( condition )        if (!(condition)) { ilog_error(IDRA_FILELINE("FALSE\n")); IDRA_DEBUG_BREAK }
#define IDRA_NOT_IMPLEMENTED_STR    __FILE__ ":" __LINE__ " not implemented!"
#define IDRA_NOT_IMPLEMENTED        { ilog_error( IDRA_FILELINE("not implemented\n") ); IDRA_DEBUG_BREAK }

#if defined(_MSC_VER)
#define iassertm( condition, message, ... ) if (!(condition)) { ilog_error(IDRA_FILELINE(message), __VA_ARGS__); IDRA_DEBUG_BREAK }
#else
#define iassertm( condition, message, ... ) if (!(condition)) { ilog_error(IDRA_FILELINE(message), ## __VA_ARGS__); IDRA_DEBUG_BREAK }
#endif

#define istatic_assert( condition, message ) static_assert( condition, #message )


} // namespace idra