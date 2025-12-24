cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-santa C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Include the PicoSystem library
# PICOSYSTEM_DIR must be set via -DPICOSYSTEM_DIR=... or environment variable
if(NOT PICOSYSTEM_DIR AND DEFINED ENV{PICOSYSTEM_DIR})
    set(PICOSYSTEM_DIR $ENV{PICOSYSTEM_DIR})
endif()
if(NOT PICOSYSTEM_DIR)
    message(FATAL_ERROR "PICOSYSTEM_DIR must be set to the picosystem SDK directory")
endif()
include(${PICOSYSTEM_DIR}/libraries/picosystem.cmake REQUIRED)

# Add compile options
add_compile_options("-Wall" "-Wextra" "-Wno-unused-parameter")

# Build the executable
picosystem_executable(
    pico-santa
    src/game.cpp
    src/render3d.cpp
    src/rasterizer.cpp
    src/city.cpp
)

# PicoSystem specific settings
pixel_double(pico-santa)
disable_startup_logo(pico-santa)
no_spritesheet(pico-santa)

# Link multicore library for dual-core rendering
target_link_libraries(pico-santa pico_multicore)

# Optimize divider in RAM for performance
target_compile_definitions(pico-santa PUBLIC PICO_DIVIDER_IN_RAM=1)
