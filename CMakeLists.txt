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

if(PROJECT_IS_TOP_LEVEL)
    install(TARGETS pixelart RUNTIME DESTINATION .)
endif()
