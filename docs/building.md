# Building NVIDIA BIOS Reader

NVIDIA BIOS Reader uses CMake and C++20. The parser and CLI are portable. The
native GUI is built only on Windows.

## Downloading the source

```bash
git clone https://github.com/fmuniztriana/nvidia-bios-reader.git
cd nvidia-bios-reader
```

## Windows

Install Visual Studio Build Tools 2022 or newer with the **Desktop development
with C++** workload. Open a Developer PowerShell in the repository and run:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The usual Visual Studio output paths are:

```text
build/Release/nvidia-bios-reader.exe
build/Release/nvidia-bios-reader-cli.exe
```

MSVC release builds use the static C++ runtime, so the two executables can be
copied without a separate Visual C++ redistributable installation.

## Ubuntu and Debian

Install the compiler and CMake:

```bash
sudo apt update
sudo apt install build-essential cmake
```

Configure, compile, and test:

```bash
cmake -S . -B build -DNVBR_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The resulting binary is:

```text
build/nvidia-bios-reader-cli
```

Run it directly:

```bash
./build/nvidia-bios-reader-cli card.rom --timings
```

Or install it system-wide:

```bash
sudo cmake --install build
nvidia-bios-reader-cli --version
```

## Fedora

```bash
sudo dnf install gcc-c++ cmake
cmake -S . -B build -DNVBR_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Arch Linux

```bash
sudo pacman -S --needed base-devel cmake
cmake -S . -B build -DNVBR_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Building an exact pre-release version

The visible version can be overridden at configure time:

```bash
cmake -S . -B build \
  -DNVBR_BUILD_GUI=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DNVBR_VERSION=0.4.0-beta.1
```

On PowerShell, quote the complete definition so punctuation is passed as one
argument:

```powershell
cmake -S . -B build '-DNVBR_VERSION=0.4.0-beta.1'
```

## Linux binary compatibility

The CI-produced Linux executable targets the current GitHub-hosted Ubuntu
runner. It is a single program file, but still uses the operating system's
standard glibc and C++ runtime. Building locally is the most reliable option
for older distributions.

## Troubleshooting

### `cmake: command not found`

Install CMake using the package command for the operating system above. On
Windows, run from a Visual Studio Developer PowerShell or add CMake to `PATH`.

### The GUI target is missing on Linux

This is expected. `src/gui.cpp` uses native Win32 controls. Linux builds
provide the complete command-line parser and comparison functionality.

### Treating experimental fields correctly

A successful build does not increase the confidence of a decoded field. Read
[Format and Confidence Notes](format-notes.md) before changing field names or
submitting a reverse-engineering conclusion.
