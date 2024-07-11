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
    shader_compiler.cpp
    ../../idra/kernel/allocator.hpp
    ../../idra/kernel/allocator.cpp
    ../../idra/kernel/array.hpp
    ../../idra/kernel/assert.hpp
    ../../idra/kernel/bit.hpp
    ../../idra/kernel/bit.cpp
    ../../idra/kernel/file.hpp
    ../../idra/kernel/file.cpp
    ../../idra/kernel/lexer.hpp
    ../../idra/kernel/lexer.cpp
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
    $ENV{VULKAN_SDK}/include
)

# Disabled for now, this is for glslang compiled from source.
set(GLSLANG_LIBS
        OSDependent
        SPIRV
        glslang
        )

if ( WIN32 )
target_link_libraries(shader_compiler PRIVATE
    ${Vulkan_LIBRARIES}
    debug $ENV{VULKAN_SDK}/Lib/glslangd.lib optimized $ENV{VULKAN_SDK}/Lib/glslang.lib
    debug $ENV{VULKAN_SDK}/Lib/SPIRVd.lib optimized $ENV{VULKAN_SDK}/Lib/SPIRV.lib
    debug $ENV{VULKAN_SDK}/Lib/MachineIndependentd.lib optimized $ENV{VULKAN_SDK}/Lib/MachineIndependent.lib
    debug $ENV{VULKAN_SDK}/Lib/OSDependentd.lib optimized $ENV{VULKAN_SDK}/Lib/OSDependent.lib
    debug $ENV{VULKAN_SDK}/Lib/GenericCodeGend.lib optimized $ENV{VULKAN_SDK}/Lib/GenericCodeGen.lib
    debug $ENV{VULKAN_SDK}/Lib/SPIRV-Toolsd.lib optimized $ENV{VULKAN_SDK}/Lib/SPIRV-Tools.lib
    debug $ENV{VULKAN_SDK}/Lib/SPIRV-Tools-Optd.lib optimized $ENV{VULKAN_SDK}/Lib/SPIRV-Tools-Opt.lib
    debug $ENV{VULKAN_SDK}/Lib/SPVRemapperd.lib optimized $ENV{VULKAN_SDK}/Lib/SPVRemapper.lib
    
    # Disabled for now, this is for glslang compiled from source.
    #${GLSLANG_LIBS}
)
else()
target_link_libraries(shader_compiler PRIVATE
    #${Vulkan_LIBRARIES}
    debug $ENV{VULKAN_SDK}/lib/libglslang.a optimized $ENV{VULKAN_SDK}/Lib/glslang.lib
    debug $ENV{VULKAN_SDK}/lib/libSPIRV.a optimized $ENV{VULKAN_SDK}/Lib/SPIRV.lib
    debug $ENV{VULKAN_SDK}/lib/libMachineIndependent.a optimized $ENV{VULKAN_SDK}/Lib/MachineIndependent.lib
    debug $ENV{VULKAN_SDK}/lib/libOSDependent.a optimized $ENV{VULKAN_SDK}/Lib/OSDependent.lib
    debug $ENV{VULKAN_SDK}/lib/libGenericCodeGen.a optimized $ENV{VULKAN_SDK}/Lib/GenericCodeGen.lib
    # libSPIRV-Tools-opt must be included before libSPIRV-Tools
    debug $ENV{VULKAN_SDK}/lib/libSPIRV-Tools-opt.a optimized $ENV{VULKAN_SDK}/Lib/SPIRV-Tools-Opt.lib
    debug $ENV{VULKAN_SDK}/lib/libSPIRV-Tools.a optimized $ENV{VULKAN_SDK}/Lib/SPIRV-Tools.lib
    debug $ENV{VULKAN_SDK}/lib/libSPVRemapper.a optimized $ENV{VULKAN_SDK}/Lib/SPVRemapper.lib
    
    # Disabled for now, this is for glslang compiled from source.
    #${GLSLANG_LIBS}
)
endif()

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output executable name based on configuration.
    # OUTPUTCONFIG used for RUNTIME_OUTPUT_NAME_ must be uppercase.
    # OUTPUT_NAME for the executable is lowercase.
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    string( TOLOWER ${OUTPUTCONFIG} OUTPUT_NAME )
    set_target_properties(shader_compiler PROPERTIES RUNTIME_OUTPUT_NAME_${OUTPUTCONFIG} "shaderc_${OUTPUT_NAME}")
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

