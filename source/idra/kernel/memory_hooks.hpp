/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#pragma once

#include "kernel/memory.hpp"

namespace idra {
namespace memory {

#define malloc( size )      idra::g_memory->global_malloc( size, 1 )
#define free( pointer )     idra::g_memory->global_free( pointer )
#define realloc( pointer, new_size ) idra::g_memory->global_realloc( pointer, new_size )

} // namespace memory
} // namespace idra