cmake_minimum_required(VERSION 3.5)

# Configuration based setup
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
# Set configuration dependant names
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output folder
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )


# Main executable
add_library( asset_compiler SHARED
    asset_compiler.hpp
    asset_compiler.cpp
    ../../idra/kernel/allocator.hpp
    ../../idra/kernel/allocator.cpp
    ../../idra/kernel/array.hpp
    ../../idra/kernel/assert.hpp
    ../../idra/kernel/bit.hpp
    ../../idra/kernel/bit.cpp
    ../../idra/kernel/blob.hpp
    ../../idra/kernel/blob.cpp
    ../../idra/kernel/file.hpp
    ../../idra/kernel/file.cpp
    ../../idra/kernel/log.hpp
    ../../idra/kernel/log.cpp
    ../../idra/kernel/memory.hpp
    ../../idra/kernel/memory.cpp
    ../../idra/kernel/platform.hpp
    ../../idra/kernel/span.hpp
    ../../idra/kernel/string_view.hpp
    ../../idra/kernel/string.hpp
    ../../idra/kernel/string.cpp
    ../../idra/kernel/time.hpp
    ../../idra/kernel/time.cpp
    ../../idra/kernel/windows_forward_declarations.hpp

    ../../external/tlsf.c
    ../../external/tlsf.h
)

set_property( TARGET asset_compiler PROPERTY CXX_STANDARD 20 )

if ( WIN32 )
    target_compile_definitions( asset_compiler PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX )
endif()

target_include_directories( asset_compiler PRIVATE
    ../../
    ../../idra
    ../../external
)

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output executable name based on configuration.
    # OUTPUTCONFIG used for RUNTIME_OUTPUT_NAME_ must be uppercase.
    # OUTPUT_NAME for the executable is lowercase.
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    string( TOLOWER ${OUTPUTCONFIG} OUTPUT_NAME )
    set_target_properties(asset_compiler PROPERTIES RUNTIME_OUTPUT_NAME_${OUTPUTCONFIG} "assetc_${OUTPUT_NAME}")
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

