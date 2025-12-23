#!/bin/bash
# Build script for pico-santa PicoSystem target

set -e

# Set PATH for MinGW (needed for picotool and pioasm)
export PATH="C:/ProgramData/mingw64/mingw64/bin:$PATH"

cd "$(dirname "$0")"

# Create build directory if it doesn't exist
if [ ! -d "build.pico" ]; then
    mkdir -p build.pico
    cd build.pico
    cmake .. -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=../../32blit-sdk/pico.toolchain \
        -DPICO_BOARD=pimoroni_picosystem \
        -DCMAKE_BUILD_TYPE=Release
else
    cd build.pico
fi

# Build
ninja

echo ""
echo "Build complete! Output: build.pico/pico-santa.uf2"
