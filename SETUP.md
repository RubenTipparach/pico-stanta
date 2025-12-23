# Pico Santa - Setup Guide

## Required Tools

| Tool | Install Command / Link |
|------|----------------------|
| **CMake** | `choco install cmake` or [cmake.org](https://cmake.org/download/) |
| **Python 3** | `choco install python` or [python.org](https://www.python.org/downloads/) |
| **ARM GCC Toolchain** | `choco install gcc-arm-embedded` |
| **MinGW (gcc/make)** | `choco install mingw` |
| **Git** | `choco install git` |
| **32blit Tools** | `pip install 32blit` |
| **Ninja** (recommended) | `choco install ninja` |

## Quick Install (if you have Chocolatey)

```batch
choco install cmake python git mingw gcc-arm-embedded ninja
pip install 32blit
```

## Required SDKs

Clone these into the parent directory (alongside your project):

```batch
cd ..
git clone https://github.com/32blit/32blit-sdk
git clone https://github.com/raspberrypi/pico-sdk
git clone https://github.com/raspberrypi/pico-extras
```

### Optional: Initialize Pico SDK Submodules

For USB support:

```batch
cd pico-sdk
git submodule update --init
cd ..
```

## Building

Run the build script:

```batch
build.bat
```

## Flashing to PicoSystem

1. Connect PicoSystem via USB-C
2. Hold **X** button and press **Power** to enter bootloader
3. Device mounts as `RPI-RP2`
4. Copy `build.pico\pico-santa.uf2` to the drive
