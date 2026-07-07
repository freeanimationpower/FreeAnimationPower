# Free Animation Power

2D Animation Software — Hybrid Vector + Raster Engine

## Overview

Free Animation Power is a professional 2D animation tool combining bitmap (raster) and vector drawing capabilities in a single application. Built with C++20 and Qt 6, it targets animators who need both traditional frame-by-frame and modern cut-out workflows.

## Current Version

**v0.2.0-dev** — Core engine complete, V2 UI with unified panels, per-frame data model, brush engine, blend modes, tableta support.

## Features

- **Hybrid Engine**: Raster brush engine (like TVPaint) + Vector Bezier engine (like Harmony)
- **Professional Brush System**: Pressure-sensitive brushes with paper texture simulation, configurable size/opacity/hardness/shape
- **Multi-layer Timeline**: Per-frame independent layer stacks, exposure sheet, onion skinning
- **Layer Blend Modes**: Normal, Multiply, Screen, Overlay, Add, Subtract, Darken, Lighten, ColorBurn, ColorDodge, SoftLight, HardLight
- **Wacom/Tablet Support**: Full pressure, tilt, and rotation support via Qt tablet events
- **Onion Skinning**: Configurable previous/next frames with red/blue tints and opacity
- **Open File Format**: Directory-based `.fap` format (PNG frames + JSON metadata)
- **Video Export**: FFmpeg integration for MP4, GIF, PNG sequence export
- **Undo/Redo**: Full command-pattern undo history for raster operations
- **UI Panels V2**: Dark-themed Qt Widgets with dockable Toolbox, Layers, Color, Properties, and Timeline panels

## Requirements

- **OS**: Windows 10/11, macOS 12+, Ubuntu 22.04+
- **Compiler**: MSVC 2022+, GCC 12+, Clang 15+ (C++20 support)
- **CMake**: 3.20+
- **Qt**: 6.5+ (Core, Gui, Widgets, OpenGL)
- **FFmpeg**: 5.0+ (for video export, optional)
- **GoogleTest**: 1.14+ (for tests)

## Quick Start

```bash
# Clone and build
git clone <repo-url>
cd "free animation power"

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run
.\build\Release\free-animation-2d-style.exe

# Run tests
cmake --build build --target fap-tests
.\build\Release\fap-tests.exe
```

## Project Structure

```
src/
├── core/          Data model (Document, Layer, Timeline, ToolState, AppState, Types)
├── engine/
│   ├── raster/       Bitmap pixel engine (RasterEngine, RasterStroke)
│   ├── vector/       Bezier path engine (VectorEngine, BezierPath)
│   ├── brush/        Brush engine with paper texture (BrushEngine, BrushPreset)
│   ├── compositor/   Layer compositing with blend modes (Compositor, NodeGraph)
│   ├── animation/    Timeline playback, onion skinning, frame thumbnails
│   ├── deformation/  Mesh deformation engine
│   └── common/       Shared utilities (blend_utils)
├── ui/           Qt widgets V1 (CanvasWidget, MainWindow, panels)
├── ui_v2/        Qt widgets V2 (CanvasWidgetV2, MainWindowV2, unified panels)
├── io/           File format (.fap), video export (FFmpeg)
└── platform/     Input handling, tablet support
```

## Architecture

```
AppState (central state manager)
├── Document (canvas size, FPS, per-frame layer stacks, undo manager)
├── ToolState (active tool, brush size/opacity/hardness/shape, pressure, stabilizer)
├── Timeline (current frame, playback state, frame count)
└── ThumbnailCache (cached frame thumbnails for timeline)

MainWindowV2
├── CanvasWidgetV2 (central drawing canvas)
├── ToolboxPanelV2 (tool palette, color swatch, onion skin, canvas settings)
├── LayerPanelV2 (layer stack, visibility/lock/blend mode)
├── ColorPanelV2 (color picker)
├── PropertyEditorV2 (brush size/opacity/hardness/shape/stabilizer/pressure)
└── TimelinePanelV2 (frame thumbnails, playback controls, FPS)
```

## Data Flow

```
Input (Mouse/Tablet) → CanvasWidgetV2
  → drawBrushStamp() → RasterLayer::setPixel()
  → commitStroke() → UndoManager (PaintCommandV2 undo/redo)
  → canvasUpdated → TimelinePanelV2::update (refresh thumbnails)
  → paintEvent → composited layers → screen
```

## Key Design Decisions

1. **Per-frame layer stacks**: Each frame has its own independent `GroupLayer` with layers (`Document::frames_`), enabling exposure sheet workflows
2. **CPU raster rendering**: Brush strokes rendered directly on CPU pixel buffers (like TVPaint), no GPU required
3. **ToolState as QObject**: Central state observable via Qt signals/slots, connected to all UI panels
4. **Zero-copy QImage wrappers**: `wrapRasterLayer()` creates QImage pointing directly to `RasterLayer::pixelData()` for paint display
5. **Command pattern undo**: `UndoCommand` base class with `undo()`/`redo()` virtual methods, used by `PaintCommandV2`

## Documentation

- `docs/architecture.md` — Full architecture overview and data flow
- `docs/build-instructions.md` — Detailed build guide for Windows/macOS/Linux
- `CONTEXT.md` — Domain language and architecture decisions
- `INVESTIGACION_TECNICA.md` — Technical research on TVPaint & FlipaClip (Spanish)
- `HANDOFF_REPORT_2026-06-18.md` — Handoff report with crash fixes and current status
- `STATUS_LENS_REFACTOR.md` — Lens Pattern refactor status
- `AGENTS.md` — AI agent instructions (skills, build commands, conventions)

## License

GPL-3.0
