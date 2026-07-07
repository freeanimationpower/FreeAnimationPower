# Free Animation Power — Build Instructions (v0.2.0-dev)

## Prerequisites

| Requirement | Minimum Version | Notes |
|-------------|-----------------|-------|
| CMake | 3.20+ | |
| Qt | 6.5+ | Components: Core, Gui, Widgets, OpenGL, Multimedia |
| FFmpeg | 4.0+ | Required for video export (optional for core build) |
| C++ Compiler | C++20 support | MSVC 2022+, GCC 11+, Clang 14+ |
| GoogleTest | 1.12+ | Only if `FAP_BUILD_TESTS=ON` (default) |

---

## Quick Build (all platforms)

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run
./build/Release/free-animation-2d-style.exe    # Windows
./build/free-animation-2d-style                # Linux/macOS

# Tests
cmake --build build --target fap-tests
./build/Release/fap-tests.exe                  # Windows
./build/fap-tests                              # Linux/macOS
```

---

## Windows (MSVC 2022)

### 1. Install Dependencies

```powershell
# Install Qt 6.5+ via Qt Installer or vcpkg
vcpkg install qt6[core,gui,widgets,opengl,multimedia]:x64-windows
vcpkg install gtest:x64-windows
vcpkg install ffmpeg:x64-windows
```

### 2. Configure and Build

```powershell
# From the project root
mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DFAP_BUILD_TESTS=ON

cmake --build . --config Release
```

### 3. Run Tests

```powershell
ctest --build-config Release --output-on-failure
```

Or directly:

```powershell
.\tests\Release\fap-tests.exe
```

---

## macOS (Clang)

### 1. Install Dependencies

```bash
# Using Homebrew
brew install cmake
brew install qt@6
brew install ffmpeg
brew install googletest
```

### 2. Configure and Build

```bash
# From the project root
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
  -DFAP_BUILD_TESTS=ON

cmake --build . -j$(sysctl -n hw.logicalcpu)
```

### 3. Run Tests

```bash
ctest --output-on-failure
# or
./fap-tests
```

---

## Linux (GCC / Clang)

### 1. Install Dependencies

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build \
  qt6-base-dev qt6-multimedia-dev libqt6opengl6-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libgtest-dev
```

**Fedora:**

```bash
sudo dnf install -y \
  cmake ninja-build gcc-c++ \
  qt6-qtbase-devel qt6-qtmultimedia-devel \
  ffmpeg-devel \
  gtest-devel
```

**Arch Linux:**

```bash
sudo pacman -S --noconfirm \
  cmake ninja gcc \
  qt6-base qt6-multimedia \
  ffmpeg \
  gtest
```

### 2. Configure and Build

```bash
# From the project root
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DFAP_BUILD_TESTS=ON

cmake --build . -j$(nproc)
```

### 3. Run Tests

```bash
ctest --output-on-failure
# or
./fap-tests
```

---

## CMake Options Reference

| Option | Default | Description |
|--------|---------|-------------|
| `FAP_BUILD_TESTS` | ON | Build GoogleTest test suite |
| `FAP_RENDERER_SKIA` | ON | Enable Skia graphics backend |
| `FAP_RENDERER_OPENGL` | ON | Enable OpenGL backend |
| `CMAKE_BUILD_TYPE` | — | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `CMAKE_PREFIX_PATH` | — | Path to Qt installation (needed on macOS/Windows) |

---

## Test Linking Note

The test executable `fap-tests` is a standalone binary that must include all implementation sources it depends on. If you encounter **unresolved symbol** errors when building tests, ensure the CMakeLists.txt test target includes the necessary `.cpp` files from `src/`. The test files depend on implementations from:

- `src/core/timeline.cpp`
- `src/core/document.cpp`
- `src/core/layer.cpp`
- `src/engine/brush/brush_engine.cpp`
- `src/engine/brush/brush_preset.cpp`
- `src/engine/vector/bezier_path.cpp`
- `src/io/file_format.cpp`

To fix, update the `TEST_SOURCES` list in `CMakeLists.txt` to include these implementation files, or add a static library target that the test links against:

```cmake
# Alternative: create a library for test dependencies
add_library(fap-core-lib
    src/core/document.cpp
    src/core/timeline.cpp
    src/core/layer.cpp
    src/engine/brush/brush_engine.cpp
    src/engine/brush/brush_preset.cpp
    src/engine/vector/bezier_path.cpp
    src/io/file_format.cpp
)
target_include_directories(fap-core-lib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(fap-tests PRIVATE fap-core-lib GTest::gtest_main Qt6::Core Qt6::Gui)
```

---

## Troubleshooting

### "Qt6 not found"

Set `CMAKE_PREFIX_PATH` to your Qt installation:

```bash
cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x.x/gcc_64"
```

On macOS with Homebrew:

```bash
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
```

### "GTest not found"

Install GoogleTest via your package manager, or let CMake fetch it:

```bash
# FetchContent approach (add to CMakeLists.txt):
include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git
                     GIT_TAG v1.14.0)
FetchContent_MakeAvailable(googletest)
```

### "C++20 not supported by compiler"

Ensure you are using:
- MSVC 2022 (19.30+) or later
- GCC 11+ 
- Clang 14+

### "LNK2019: unresolved external symbol" (Windows)

The test executable is missing implementation source files. See **Test Linking Note** above. Add the required `.cpp` files to the test target.

### "fap-tests not found after build"

Ensure `FAP_BUILD_TESTS=ON` was passed to CMake and that GoogleTest was found. Check the CMake output for `Found GTest: ...`.
