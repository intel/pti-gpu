# Building and Installation Guide

This guide provides instructions for building and installing the PTI SDK.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Building from Source](#building-from-source)
  - [Linux](#linux)
  - [Windows](#windows)
- [Installation](#installation)
- [Verification](#verification)
- [Linking PTI SDK in Your Application](#linking-pti-sdk-in-your-application)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Common Requirements

- **CMake**: Version 3.14 or higher (3.20+ for CMake presets, 3.23+ for Windows tests)
- **Git**: Version 1.8 or higher
- **Ninja**: Version 1.10.1 or higher (optional for Linux, required for Windows)
- **Intel(R) oneAPI Base Toolkit**: See [tested versions](#tested-versions) below
- **Level Zero Loader**: Version 1.28.0 or higher

### Platform-Specific Requirements

#### Linux
- GCC or Intel C++ Compiler (icpx)
- Make or Ninja build system

#### Windows
- Visual Studio 2022 or higher
- Ninja build system (version 1.12.1 or higher)
- Intel C++ Compiler (icpx)

### Tested Versions

#### Intel(R) oneAPI Base Toolkit

| Support | Linux | Windows |
| -------- | ------- | ------- |
| Full | 2025.1.0 - 2025.3.0 | 2025.1.0 - 2025.3.0 |
| Tested | 2026.0 | 2026.0 |

**Note**: Full support indicates regular testing with all features. Tested versions may have limited validation.

#### Regularly Tested Configurations

- Ubuntu 22.04 with Intel(R) Data Center GPU Max 1550
- Ubuntu 22.04 with Intel(R) Data Center GPU Max 1100
- Ubuntu 24.04 with Intel(R) Data Center GPU Max Series
- Ubuntu 25.10 with Intel(R) Arc(TM) Graphics (A-Series and B-Series)
- Rocky 9 with Intel(R) Data Center GPU Max Series
- Windows 11 with Intel(R) Arc(TM) Graphics (A-Series and B-Series)
- Windows 11 with Intel(R) Data Center GPU Flex Series

## Building from Source

### Environment Setup

**Linux:**
```bash
source <path_to_oneapi>/setvars.sh
cd sdk
```

**Windows:**
```cmd
call "C:\Program Files (x86)\Intel\oneAPI\setvars-vcvarsall.bat"
call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
cd sdk
```

### Configure Build

**Configure build with testing (debug):**
```bash
CC=icx CXX=icpx cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

**Configure build with testing (release):**
```bash
CC=icx CXX=icpx cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

**Note:** The Intel C++ Compiler (icpx) is recommended. You can also build with GCC by omitting the `CC=icx CXX=icpx` prefix.

### Build

```bash
cmake --build build --parallel 4
```

## Verification

### Running Tests

From the build directory:

```bash
ctest --test-dir build --output-on-failure
```

## Installation

Most users do not need to install PTI SDK. Installation is only necessary if you want to:

* Install PTI SDK system-wide or to a custom location
* Use PTI SDK with CMake ``find_package()`` in other projects
* Package PTI SDK for distribution

From the build directory:

```bash
cmake --install build --config Release --prefix "../out"
```

You can specify a different installation prefix:

**Linux:**
```bash
cmake --install build --config Release --prefix "/usr/local"
```

**Windows:**
```cmd
cmake --install build --config Release --prefix "C:\Program Files\PTI"
```

The installation includes:
- Header files in `<prefix>/include/pti/`
- Library files in `<prefix>/lib/`
- CMake configuration files in `<prefix>/lib/cmake/pti/`

### Running Sample Applications

**Linux:**
```bash
./build/bin/vector_sq_add
```

**Windows:**
```cmd
build\bin\vector_sq_add.exe
```

If the sample runs successfully and produces output, your installation is working correctly.

## Linking PTI SDK in Your Application

### Using CMake find_package

Add the following to your `CMakeLists.txt`:

```cmake
# Set Pti_DIR if you installed in a non-standard location
set(Pti_DIR <path-to-pti>/lib/cmake/pti)

# Find the package
find_package(Pti REQUIRED)

# Link against the pti_view library
target_link_libraries(your_target PUBLIC Pti::pti_view)
```

### Manual Linking

If not using CMake's `find_package`, you can manually specify:

- **Include directory**: `<install-prefix>/include`
- **Library directory**: `<install-prefix>/lib`
- **Library to link**: `pti_view`

Example with g++:

```bash
g++ -I<install-prefix>/include \
    -L<install-prefix>/lib \
    -lpti_view \
    your_application.cpp -o your_app
```

## Troubleshooting

### Common Issues

**CMake cannot find oneAPI components:**
- Ensure you've sourced the oneAPI environment:
  - Linux: `source <path_to_oneapi>/setvars.sh`
  - Windows: Run `<path_to_oneapi>\setvars.bat` in cmd

**Level Zero headers not found:**
- Verify Level Zero Loader is installed (version >= 1.28.0)
- Ensure oneAPI environment is properly sourced/initialized

**Build fails with compiler errors:**
- Ensure you're using a supported oneAPI version
- Verify C++17 support is available
- Try deleting the build directory and reconfiguring

**Windows: Kernel name demangling not available:**
- This is a known limitation on Windows
- Kernel names will appear in mangled form

### Getting Help

- Submit issues at: https://github.com/intel/pti-gpu/issues
- Check existing issues and discussions
- Refer to the samples directory for usage examples

## Additional Build Options

### Common CMake Options

- `-DCMAKE_BUILD_TYPE=<Debug|Release>`: Set build type
- `-DCMAKE_CXX_STANDARD=<17|20>`: Set C++ standard (minimum 17)
- `-DCMAKE_CXX_COMPILER=<compiler>`: Set C++ compiler (e.g., g++, icpx)
- `-DCMAKE_C_COMPILER=<compiler>`: Set C compiler (e.g., gcc, icx)
- `-DBUILD_TESTING=<ON|OFF>`: Enable/disable building tests (default: ON)


### PTI Custom Options

- `-DPTI_BUILD_SAMPLES=<ON|OFF>`: Enable/disable building samples (default: ON when building as top-level project)
- `-DPTI_BUILD_TESTING=<ON|OFF>`: Enable/disable building PTI tests (default: ON when building as top-level project)
- `-DPTI_INSTALL=<ON|OFF>`: Enable/disable library installation when invoking `--install` (default: ON)
- `-DPTI_ENABLE_LOGGING=<ON|OFF>`: Enable internal logging for PTI (default: OFF)

---

For more information, see the [PTI SDK Documentation](https://intel.github.io/pti-gpu/).
