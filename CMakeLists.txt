cmake_minimum_required(VERSION 3.5)

project(devgames_2024 VERSION 0.4.0)

find_package( Vulkan REQUIRED )

if ( UNIX )
    find_package( SDL2 REQUIRED )
endif()


# Configuration based setup
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
# Set configuration dependant names
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output folder
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )


# Main executable
add_executable( devgames_2024
    source/devgames_2024/main.cpp

    source/idra/kernel/allocator.hpp
    source/idra/kernel/allocator.cpp
    source/idra/kernel/array.hpp
    source/idra/kernel/assert.hpp
    source/idra/kernel/asset.hpp
    source/idra/kernel/asset.cpp
    source/idra/kernel/bit.hpp
    source/idra/kernel/bit.cpp
    source/idra/kernel/blob.hpp
    source/idra/kernel/blob.cpp
    source/idra/kernel/camera.hpp
    source/idra/kernel/camera.cpp
    source/idra/kernel/color.hpp
    source/idra/kernel/color.cpp
    source/idra/kernel/file.hpp
    source/idra/kernel/file.cpp
    source/idra/kernel/hash_map.hpp
    source/idra/kernel/input.hpp
    source/idra/kernel/input.cpp
    source/idra/kernel/log.hpp
    source/idra/kernel/log.cpp
    source/idra/kernel/memory.hpp
    source/idra/kernel/memory.cpp
    source/idra/kernel/memory_hooks.hpp
    source/idra/kernel/memory_hooks.cpp
    source/idra/kernel/numerics.hpp
    source/idra/kernel/numerics.cpp
    source/idra/kernel/pool.hpp
    source/idra/kernel/pool.cpp
    source/idra/kernel/platform.hpp
    source/idra/kernel/relative_data_structures.hpp
    source/idra/kernel/span.hpp
    source/idra/kernel/string_view.hpp
    source/idra/kernel/string.hpp
    source/idra/kernel/string.cpp
    source/idra/kernel/task_manager.hpp
    source/idra/kernel/task_manager.cpp
    source/idra/kernel/thread.hpp
    source/idra/kernel/thread.cpp
    source/idra/kernel/time.hpp
    source/idra/kernel/time.cpp
    source/idra/kernel/utf.hpp
    source/idra/kernel/windows_forward_declarations.hpp

    source/idra/gpu/command_buffer.hpp
    source/idra/gpu/command_buffer.cpp
    source/idra/gpu/gpu_device.hpp
    source/idra/gpu/gpu_device_vulkan.cpp
    source/idra/gpu/gpu_enums.hpp
    source/idra/gpu/gpu_profiler.hpp
    source/idra/gpu/gpu_profiler.cpp
    source/idra/gpu/gpu_resources.hpp
    source/idra/gpu/gpu_resources.cpp
    source/idra/gpu/idra_imgui.hpp
    source/idra/gpu/idra_imgui.cpp
    source/idra/gpu/vulkan_forward_declarations.hpp

    source/idra/graphics/atmospheric_scattering.hpp
    source/idra/graphics/atmospheric_scattering.cpp
    source/idra/graphics/crt_post_process.hpp
    source/idra/graphics/crt_post_process.cpp
    source/idra/graphics/debug_renderer.hpp
    source/idra/graphics/debug_renderer.cpp
    source/idra/graphics/gpu_debug_print_system.hpp
    source/idra/graphics/gpu_debug_print_system.cpp
    source/idra/graphics/graphics_asset_loaders.hpp
    source/idra/graphics/graphics_asset_loaders.cpp
    source/idra/graphics/graphics_blueprints.hpp
    source/idra/graphics/graphics_blueprints.cpp
    source/idra/graphics/render_system_interface.hpp
    source/idra/graphics/sprite_animation.cpp
    source/idra/graphics/sprite_animation.hpp
    source/idra/graphics/sprite_batch.hpp
    source/idra/graphics/sprite_batch.cpp
    source/idra/graphics/sprite_render_system.hpp
    source/idra/graphics/sprite_render_system.cpp

    source/idra/imgui/imgui_helpers.hpp
    source/idra/imgui/imgui_helpers.cpp
    source/idra/imgui/widgets.hpp
    source/idra/imgui/widgets.cpp

    source/idra/application/application.hpp
    source/idra/application/application.cpp
    source/idra/application/game_camera.hpp
    source/idra/application/game_camera.cpp
    source/idra/application/window.hpp
    source/idra/application/window.cpp

    source/external/tlsf.c
    source/external/tlsf.h
    source/external/volk.h
    source/external/volk.c
    source/external/vk_mem_alloc.h
    source/external/imgui/imgui.cpp
    source/external/imgui/imgui_draw.cpp
    source/external/imgui/imgui_tables.cpp
    source/external/imgui/imgui_widgets.cpp
    source/external/imgui/backends/imgui_impl_sdl2.cpp
)

set_property( TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20 )

if ( WIN32 )
    target_compile_definitions( ${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        VK_USE_PLATFORM_WIN32_KHR
        IDRA_VULKAN
        IDRA_IMGUI )
else()
    target_compile_definitions( ${PROJECT_NAME} PRIVATE
        IDRA_VULKAN
        IDRA_IMGUI )
endif()

target_include_directories( ${PROJECT_NAME} PRIVATE
    source/
    source/devgames_2024
    source/idra
    source/external
    source/external/imgui
)

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    # Output executable name based on configuration.
    # OUTPUTCONFIG used for RUNTIME_OUTPUT_NAME_ must be uppercase.
    # OUTPUT_NAME for the executable is lowercase.
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    string( TOLOWER ${OUTPUTCONFIG} OUTPUT_NAME )
    set_target_properties(devgames_2024 PROPERTIES RUNTIME_OUTPUT_NAME_${OUTPUTCONFIG} "devgames_2024_demo_${OUTPUT_NAME}")
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# glslang
# Disabled as we are now using the Vulkan libraries directly.
# Leaving it there if needed in the future.
#set(BUILD_SHARED_LIBS FALSE)
#set(BUILD_EXTERNAL TRUE)
#set(ENABLE_GLSLANG_BINARIES FALSE)
#set(ENABLE_CTEST FALSE)
#set(ENABLE_OPT FALSE)
#add_subdirectory(source/external/glslang)
#include_directories(source/external/glslang)


if (WIN32)
    target_link_directories(${PROJECT_NAME} PRIVATE
        source/external/SDL2-2.28.5/lib/x64
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        source/external/SDL2-2.28.5/include)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        $ENV{VULKAN_SDK}/include
        ${SDL2_INCLUDE_DIRS})
endif()


if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        dl
        pthread
        SDL2::SDL2)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    shader_compiler
    asset_compiler
    # Vulkan libraries are not needed - volk takes care of it.
    #${Vulkan_LIBRARIES}
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# Copy dlls to binary folder
if (WIN32)
    set(DLLS_TO_COPY
        ${CMAKE_CURRENT_SOURCE_DIR}/source/external/SDL2-2.28.5/lib/x64/SDL2.dll
    )

    foreach(DLL ${DLLS_TO_COPY})
        add_custom_command(TARGET Extra ${PROJECT_NAME}
            COMMAND ${CMAKE_COMMAND} -E copy ${DLL} "$(OutDir)"
            VERBATIM
        )
    endforeach()
endif()

# Add other projects
add_subdirectory(source/tools/shader_compiler)
add_subdirectory(source/tools/asset_compiler)
