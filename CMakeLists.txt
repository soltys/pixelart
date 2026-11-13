cmake_minimum_required(VERSION 4.0)

# Static CRT so pixelart.exe is one self-contained file. Must be set before
# project(); a parent project (e.g. md) may already have set it.
cmake_policy(SET CMP0091 NEW)
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(pixelart VERSION 1.0.0 LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "pixelart is a Windows-only tool.")
endif()
if(NOT MSVC)
    message(WARNING "pixelart targets the MSVC toolchain.")
endif()

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Text sprite sheets -> PNG/ICO. The only dependency, stb_image_write, is
# vendored in third_party/stb.
# Every palettes/*.txt is compiled into the exe (see cmake/EmbedPalettes.cmake),
# so `pixelart -p db32` works with nothing next to pixelart.exe.
file(GLOB PIXELART_PALETTES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/palettes/*.txt")
set(PIXELART_PALETTES_H "${PROJECT_BINARY_DIR}/generated/palettes.h")
add_custom_command(
    OUTPUT "${PIXELART_PALETTES_H}"
    COMMAND "${CMAKE_COMMAND}" "-DPALETTE_DIR=${PROJECT_SOURCE_DIR}/palettes"
            "-DOUT=${PIXELART_PALETTES_H}" -P "${PROJECT_SOURCE_DIR}/cmake/EmbedPalettes.cmake"
    DEPENDS ${PIXELART_PALETTES} "${PROJECT_SOURCE_DIR}/cmake/EmbedPalettes.cmake"
    COMMENT "Embedding palettes/*.txt"
    VERBATIM)

add_executable(pixelart src/pixelart.c "${PIXELART_PALETTES_H}")
set_target_properties(pixelart PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_include_directories(pixelart PRIVATE third_party/stb "${PROJECT_BINARY_DIR}/generated")
target_compile_definitions(pixelart PRIVATE
    PIXELART_VERSION="${PROJECT_VERSION}"
    UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

if(MSVC)
    target_compile_options(pixelart PRIVATE /W4 /utf-8 /permissive-)
    target_link_options(pixelart PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO)
else()
    target_compile_options(pixelart PRIVATE -municode -Wall -Wextra -Wno-unused-function)
    target_link_options(pixelart PRIVATE -municode)
endif()

# `cmake --build --preset release --target examples` renders every sheet in
# examples/ into <build>/examples. The palette comes from the file name:
# examples/db16.txt uses the bundled db16 palette, examples/glass.txt uses
# examples/glass-palette.txt, anything else the default PICO-8.
file(GLOB PIXELART_EXAMPLES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/*.txt")
list(FILTER PIXELART_EXAMPLES EXCLUDE REGEX "-palette\\.txt$")
set(PIXELART_EXAMPLES_OUT "${PROJECT_BINARY_DIR}/examples")
set(PIXELART_EXAMPLE_COMMANDS "")
foreach(sheet IN LISTS PIXELART_EXAMPLES)
    get_filename_component(name "${sheet}" NAME_WE)
    set(palette_args "")
    set(palette_file "${PROJECT_SOURCE_DIR}/examples/${name}-palette.txt")
    if(EXISTS "${palette_file}")
        set(palette_args -p "${palette_file}")
    elseif(EXISTS "${PROJECT_SOURCE_DIR}/palettes/${name}.txt")
        set(palette_args -p "${name}")
    endif()
    list(APPEND PIXELART_EXAMPLE_COMMANDS
        COMMAND $<TARGET_FILE:pixelart> ${palette_args} -o "${PIXELART_EXAMPLES_OUT}" "${sheet}")
endforeach()
add_custom_target(examples
    ${PIXELART_EXAMPLE_COMMANDS}
    DEPENDS pixelart
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Rendering examples/*.txt into ${PIXELART_EXAMPLES_OUT}"
    VERBATIM)

if(PROJECT_IS_TOP_LEVEL)
    install(TARGETS pixelart RUNTIME DESTINATION .)
endif()
