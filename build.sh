#!/bin/bash
# Build script for MSYS2 UCRT64 environment

echo "=== Pico Santa Build Script (MSYS2) ==="
echo

# Clean old build
if [ -d "build.pico" ]; then
    echo "Cleaning old build directory..."
    rm -rf build.pico
fi

mkdir build.pico
cd build.pico

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=../../32blit-sdk/pico.toolchain \
    -DPICO_BOARD=pimoroni_picosystem \
    -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

echo
echo "Building (this may take a few minutes)..."
ninja

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo
echo "=== Build Complete! ==="
echo "Output file: build.pico/pico-santa.uf2"
echo
echo "To flash to PicoSystem:"
echo "1. Hold X button and press Power to enter bootloader"
echo "2. Copy pico-santa.uf2 to the RPI-RP2 drive"
