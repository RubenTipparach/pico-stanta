@echo off
setlocal

echo === Pico Santa Web Build Script ===
echo.

:: Check if Emscripten is installed
if not exist "..\emsdk\upstream\emscripten\emcmake.py" (
    echo ERROR: Emscripten not found at ..\emsdk
    echo Run setup.bat first or install Emscripten manually.
    pause
    exit /b 1
)

:: Remove old build directory
if exist build.web (
    echo Cleaning old build directory...
    rmdir /s /q build.web
)

mkdir build.web
cd build.web

:: Run CMake configuration for Emscripten
echo Configuring with CMake for Emscripten...
python c:/Users/santi/repos/emsdk/upstream/emscripten/emcmake.py cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -D32BLIT_DIR=c:/Users/santi/repos/32blit-sdk

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo Building (this may take a few minutes on first run)...
python c:/Users/santi/repos/emsdk/upstream/emscripten/emmake.py make -j4
if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo === Build Complete! ===
echo.

:: Copy web files to dist folder for GitHub Pages
echo Copying files to dist folder...
cd ..
if not exist dist mkdir dist
copy build.web\pico-santa.html dist\index.html >nul
copy build.web\pico-santa.js dist\pico-santa.js >nul
copy build.web\pico-santa.wasm dist\pico-santa.wasm >nul

echo.
echo Output files in dist/:
echo   - index.html
echo   - pico-santa.js
echo   - pico-santa.wasm
echo.
echo Run 'run-web.bat' to test locally.
echo Push to GitHub to deploy to GitHub Pages.
echo.

pause
