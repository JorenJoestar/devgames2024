cmake_minimum_required(VERSION 3.5)

find_package( Vulkan REQUIRED )


# Configuration based setup
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
# Set configuration dependant names
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output folder
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )


# Main executable
add_library( shader_compiler SHARED
    shader_compiler.hpp
    main.cpp
    ../../idra/kernel/allocator.hpp
    ../../idra/kernel/allocator.cpp
    ../../idra/kernel/array.hpp
    ../../idra/kernel/assert.hpp
    ../../idra/kernel/bit.hpp
    ../../idra/kernel/bit.cpp
    ../../idra/kernel/file.hpp
    ../../idra/kernel/file.cpp
    ../../idra/kernel/log.hpp
    ../../idra/kernel/log.cpp
    ../../idra/kernel/memory.hpp
    ../../idra/kernel/memory.cpp
    ../../idra/kernel/platform.hpp
    ../../idra/kernel/span.hpp
    ../../idra/kernel/string_span.hpp
    ../../idra/kernel/string.hpp
    ../../idra/kernel/string.cpp
    ../../idra/kernel/time.hpp
    ../../idra/kernel/time.cpp
    ../../idra/kernel/windows_declarations.hpp

    ../../external/tlsf.c
    ../../external/tlsf.h
)

set_property( TARGET shader_compiler PROPERTY CXX_STANDARD 20 )

if ( WIN32 )
    target_compile_definitions( shader_compiler PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX )
endif()

target_include_directories( shader_compiler PRIVATE
    ../../
    ../../idra
    ../../external
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(shader_compiler PRIVATE

    ${Vulkan_LIBRARIES}
)

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output executable name based on configuration.
    # OUTPUTCONFIG used for RUNTIME_OUTPUT_NAME_ must be uppercase.
    # OUTPUT_NAME for the executable is lowercase.
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    string( TOLOWER ${OUTPUTCONFIG} OUTPUT_NAME )
    set_target_properties(shader_compiler PROPERTIES RUNTIME_OUTPUT_NAME_${OUTPUTCONFIG} "shaderc_${OUTPUT_NAME}")
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
