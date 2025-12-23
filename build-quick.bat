@echo off
setlocal

:: Set ARM toolchain path explicitly
set PICO_TOOLCHAIN_PATH=C:\ProgramData\chocolatey\lib\gcc-arm-embedded\tools\gcc-arm-none-eabi-10.3-2021.10

:: Add MinGW to PATH so pioasm and picotool can find their DLLs
set PATH=C:\ProgramData\mingw64\mingw64\bin;%PATH%

:: Create build directory if needed
if not exist build.pico (
    mkdir build.pico
)
cd build.pico

:: Run CMake configuration if needed
if not exist build.ninja (
    echo Configuring with CMake...
    cmake .. -G Ninja ^
        -DCMAKE_TOOLCHAIN_FILE=../../32blit-sdk/pico.toolchain ^
        -DPICO_BOARD=pimoroni_picosystem ^
        -DPICO_TOOLCHAIN_PATH="%PICO_TOOLCHAIN_PATH%" ^
        -DCMAKE_BUILD_TYPE=Release
    if %errorlevel% neq 0 (
        echo CMake configuration failed!
        exit /b 1
    )
)

echo Building...
ninja
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build complete: build.pico\pico-santa.uf2
