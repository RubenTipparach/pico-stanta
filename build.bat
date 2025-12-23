@echo off
setlocal

echo === Pico Santa Build Script ===
echo.

:: Set ARM toolchain path explicitly
set PICO_TOOLCHAIN_PATH=C:\ProgramData\chocolatey\lib\gcc-arm-embedded\tools\gcc-arm-none-eabi-10.3-2021.10

:: Add MinGW to PATH so pioasm and picotool can find their DLLs
set PATH=C:\ProgramData\mingw64\mingw64\bin;%PATH%

:: Remove old build directory to start fresh
if exist build.pico (
    echo Cleaning old build directory...
    rmdir /s /q build.pico
)

mkdir build.pico
cd build.pico

:: Run CMake configuration with Ninja generator
echo Configuring with CMake (using Ninja)...
cmake .. -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE=../../32blit-sdk/pico.toolchain ^
    -DPICO_BOARD=pimoroni_picosystem ^
    -DPICO_TOOLCHAIN_PATH="%PICO_TOOLCHAIN_PATH%" ^
    -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo Building (this may take a few minutes)...
ninja
if %errorlevel% neq 0 (
    echo.
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo === Build Complete! ===
echo Output file: build.pico\pico-santa.uf2
echo.
echo To flash to PicoSystem:
echo 1. Hold X button and press Power to enter bootloader
echo 2. Copy pico-santa.uf2 to the RPI-RP2 drive
echo.

pause
