@echo off
setlocal

echo === Pico Santa Development Setup ===
echo.

:: Check for Chocolatey
where choco >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Chocolatey not found!
    echo Install from https://chocolatey.org/install
    pause
    exit /b 1
)

echo [1/6] Installing build tools...
choco install -y cmake ninja git python mingw gcc-arm-embedded

echo.
echo [2/6] Installing 32blit tools...
pip install 32blit

echo.
echo [3/6] Cloning SDKs (if not present)...
cd ..

if not exist 32blit-sdk (
    echo Cloning 32blit-sdk...
    git clone https://github.com/32blit/32blit-sdk
) else (
    echo 32blit-sdk already exists, skipping...
)

if not exist pico-sdk (
    echo Cloning pico-sdk...
    git clone https://github.com/raspberrypi/pico-sdk
) else (
    echo pico-sdk already exists, skipping...
)

if not exist pico-extras (
    echo Cloning pico-extras...
    git clone https://github.com/raspberrypi/pico-extras
) else (
    echo pico-extras already exists, skipping...
)

echo.
echo [4/6] Initializing pico-sdk submodules...
cd pico-sdk
git submodule update --init
cd ..

echo.
echo [5/6] Setting up Emscripten for web builds...
if not exist emsdk (
    echo Cloning emsdk...
    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    echo Installing latest Emscripten...
    call emsdk install latest
    call emsdk activate latest
    cd ..
) else (
    echo emsdk already exists, skipping clone...
    cd emsdk
    call emsdk activate latest
    cd ..
)

cd pico-santa

echo.
echo [6/6] Setup complete!
echo.
echo === Summary ===
echo.
echo To build for PicoSystem hardware:
echo   build.bat
echo.
echo To build for web (Emscripten):
echo   ..\emsdk\emsdk_env.bat
echo   build-web.bat
echo.
echo To flash to PicoSystem:
echo   1. Hold X button and press Power
echo   2. Copy build.pico\pico-santa.uf2 to RPI-RP2 drive
echo.

pause
