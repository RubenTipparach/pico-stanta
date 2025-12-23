@echo off
setlocal

echo === Pico Santa Web Server ===
echo.

if not exist "build.web\pico-santa.html" (
    echo ERROR: Web build not found!
    echo Run build-web.bat first.
    pause
    exit /b 1
)

cd build.web

echo Starting local server at http://localhost:8000/pico-santa.html
echo Press Ctrl+C to stop the server.
echo.

:: Open browser automatically
start http://localhost:8000/pico-santa.html

:: Start the server
python -m http.server 8000
