# Free Animation Power

2D Animation Software — Hybrid Vector + Raster Engine

## Overview

Free Animation Power is a professional 2D animation tool combining bitmap (raster) and vector drawing capabilities in a single application. Built with C++20 and Qt 6, it targets animators who need both traditional frame-by-frame and modern cut-out workflows.

## Features

- **Hybrid Engine**: Raster brush engine (like TVPaint) + Vector Bezier engine (like Harmony)
- **Professional Brush System**: Pressure-sensitive brushes with paper texture simulation
- **Multi-layer Timeline**: Exposure sheet, keyframes, onion skinning, light table
- **Wacom/Tablet Support**: Full pressure, tilt, and rotation support
- **Onion Skinning**: Configurable previous/next frames with color tints
- **Open File Format**: ZIP-based `.fap` format (PNG frames + JSON metadata)
- **Video Export**: FFmpeg integration for MP4, GIF, PNG sequence export

## Requirements

- **OS**: Windows 10/11, macOS 11+, Ubuntu 22.04+
- **Compiler**: GCC 12+, Clang 15+, or MSVC 2022+
- **CMake**: 3.20+
- **Qt**: 6.5+ (Core, Gui, Widgets, OpenGL, Multimedia)
- **FFmpeg**: 5.0+ (for video export)
- **GoogleTest**: 1.14+ (for tests)

## Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run
./build/free-animation-power

# Run tests
cmake --build build --target fap-tests
```

## Architecture

```
src/
├── core/       Data model (Document, Layer, Timeline, Types)
├── engine/
│   ├── raster/     Bitmap pixel engine (tile cache, stroke rendering)
│   ├── vector/     Bezier path engine
│   ├── brush/      Brush engine with paper texture
│   └── animation/  Timeline playback, onion skinning
├── ui/         Qt widgets (Canvas, Panels, MainWindow)
├── io/         File format (.fap), video export
└── platform/   Input handling, tablet support
```

## Documentation

- `docs/architecture.md` — Full architecture overview
- `docs/build-instructions.md` — Detailed build guide
- `docs/file-format-spec.md` — .fap file format specification
- `INVESTIGACION_TECNICA.md` — Technical research (Spanish)

## License

GPL-3.0
