# Free Animation Power — Architecture Document

## Overview

Free Animation Power is a hybrid 2D animation application that combines raster and vector drawing engines into a single unified toolset. It is designed for traditional frame-by-frame animation, digital painting, and vector illustration workflows. The application targets desktop platforms (Windows, macOS, Linux) and is built with C++20, Qt 6, and CMake.

The core philosophy is a **hybrid engine architecture**: every brush stroke can be rendered as pixels (raster), as a vector path, or as both simultaneously. This allows artists to use the strengths of each pipeline without switching tools.

---

## Layer Architecture

```
+------------------------------------------------------------------+
|                          UI Layer (Qt 6)                         |
|  main_window | canvas_widget | timeline_panel | toolbox_panel    |
|  color_panel | layer_panel   | property_editor                   |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                       Engine Layer                                |
|  +------------------+  +------------------+  +----------------+  |
|  | brush/            |  | raster/           |  | vector/        |  |
|  | BrushEngine       |  | RasterEngine     |  | VectorEngine   |  |
|  | BrushPreset       |  | RasterStroke     |  | VectorStroke   |  |
|  | BrushTip          |  |                  |  | BezierPath     |  |
|  | BrushDynamics     |  |                  |  |                |  |
|  +------------------+  +------------------+  +----------------+  |
|  +-------------------------------------------------------------+ |
|  | animation/                                                   | |
|  | TimelineEngine | OnionSkin | Playback                        | |
|  +-------------------------------------------------------------+ |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                      Core Data Model                             |
|  Document | Timeline | Layer (Raster/Vector/Group) | Canvas      |
|  Project | Stroke | Types (Vec2, Color, Rect, etc.)              |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                        Platform Layer                             |
|  IO (document_io, video_export, file_format)                     |
|  Platform Input (tablet_handler, input_manager)                  |
+------------------------------------------------------------------+
```

---

## Module Descriptions

### `src/core/` — Core Data Model

| File | Description |
|------|-------------|
| `types.hpp` / `types.cpp` | Fundamental types: `Vec2`, `Color`, `Rect`, `StrokePoint`, enums (`LayerType`, `BlendMode`, `BrushMode`), and utility base classes (`NonCopyable`, `NonMovable`). |
| `document.hpp` / `document.cpp` | The top-level `Document` class. Holds canvas dimensions, FPS, frame count, modification state, and the root `GroupLayer`. |
| `timeline.hpp` / `timeline.cpp` | `Timeline` manages the current frame, frame range, playback state, looping, and per-layer keyframe tracking via a `vector<vector<bool>>`. |
| `layer.hpp` / `layer.cpp` | Layer hierarchy: abstract `Layer` base, `RasterLayer` (with pixel dimensions), `VectorLayer` (stroke collection), and `GroupLayer` (composite with children). |
| `stroke.hpp` / `stroke.cpp` | Stroke data container for brush input points. |
| `canvas.hpp` / `canvas.cpp` | `Canvas` manages the viewport transform, zoom, pan, and rotation. |
| `project.hpp` / `project.cpp` | `Project` aggregates `Document`, open file paths, and recent files. |

### `src/engine/brush/` — Brush Engine

| File | Description |
|------|-------------|
| `brush_engine.hpp` / `brush_engine.cpp` | `BrushEngine` — the central brush state machine. Manages the active `BrushPreset`, handles `beginStroke` / `addPoint` / `endStroke`, and persists/loads presets via a JSON format. |
| `brush_preset.cpp` | Factory functions for built-in presets: Round Soft, Round Hard, Flat, Calligraphy, Eraser. |
| `paper_texture.cpp` | Paper texture overlay and grain simulation. |

**Key types:**
- `BrushTip` — stamp image, spacing, hardness, angle, roundness
- `BrushDynamics` — pressure/size/opacity/flow response curves, tilt handling, smoothing
- `BrushPreset` — composed of tip + dynamics + color + blend mode + paper texture settings

### `src/engine/raster/` — Raster Engine

| File | Description |
|------|-------------|
| `raster_engine.hpp` / `raster_engine.cpp` | `RasterEngine` renders brush stamps into a pixel buffer. Supports alpha blending, blend modes, and real-time preview. |
| `raster_stroke.cpp` | `RasterStroke` — a single raster stroke with its point array, rendered as a series of dab stamps or connected segments. |

### `src/engine/vector/` — Vector Engine

| File | Description |
|------|-------------|
| `bezier_path.hpp` / `bezier_path.cpp` | `BezierPath` — cubic Bezier path with `moveTo`, `lineTo`, `curveTo`, `closePath`. Supports `evaluateAt(t)`, `tangentAt(t)`, `flattenPoints(tolerance)`, and `totalLength()`. Also provides path construction from raw `StrokePoint` arrays. |
| `vector_engine.hpp` / `vector_engine.cpp` | `VectorEngine` — manages vector strokes, path simplification, and rendering to OpenGL. |
| `vector_stroke.cpp` | `VectorStroke` — pairs a `BezierPath` with rendering attributes (color, width, opacity, blend mode). |

### `src/engine/animation/` — Animation Engine

| File | Description |
|------|-------------|
| `timeline_engine.cpp` | Drives frame advancement, handles frame-change callbacks. |
| `onion_skin.cpp` | Onion skinning: renders previous/next frames with configurable opacity and tint for traditional animation workflows. |
| `playback.cpp` | Real-time playback loop with frame timing and vsync integration. |

### `src/ui/` — User Interface (Qt 6 Widgets)

| File | Description |
|------|-------------|
| `main_window.hpp` / `.cpp` | Application main window with menu bar, toolbars, dock widgets, and central canvas. |
| `canvas_widget.hpp` / `.cpp` | OpenGL-accelerated canvas widget for drawing input and frame display. |
| `timeline_panel.hpp` / `.cpp` | Timeline panel showing frame thumbnails, keyframe dots, and playback controls. |
| `toolbox_panel.hpp` / `.cpp` | Tool palette: brush, eraser, fill, selection, eyedropper, pan, zoom. |
| `color_panel.hpp` / `.cpp` | Color picker with HSV wheel, sliders, palette presets, and opacity control. |
| `layer_panel.hpp` / `.cpp` | Layer stack with drag-to-reorder, visibility toggles, opacity sliders, and blend mode dropdowns. |
| `property_editor.hpp` / `.cpp` | Context-sensitive property editor for brush settings, layer properties, and tool options. |

### `src/io/` — Input/Output

| File | Description |
|------|-------------|
| `file_format.cpp` | `.fap` format serializer/deserializer. Uses a directory-based format with `document.json` (metadata + layer tree) and a `frames/` subdirectory with per-layer PNG frame images. |
| `document_io.cpp` | High-level document save/open with format detection and recent-files management. |
| `video_export.cpp` | Export animation to video files (MP4, GIF) using FFmpeg. |

### `src/platform/` — Platform Abstraction

| File | Description |
|------|-------------|
| `input_manager.cpp` / `input_manager.hpp` | Keyboard and mouse input normalization across platforms. |
| `tablet_handler.cpp` / `tablet_handler.hpp` | Graphics tablet (Wacom, etc.) support: pen pressure, tilt, rotation, and eraser detection. |

---

## Data Flow

```
Input (Mouse/Tablet)
    |
    v
InputManager / TabletHandler  ──►  StrokePoint stream
    |
    v
BrushEngine  ──►  beginStroke(color, size)  ──►  addPoint(pt) × N  ──►  endStroke()
    |                     |
    |               currentStroke()
    v                     v
RasterEngine          VectorEngine
    |                     |
    |  rasterize stamps    |  createPathFromPoints()
    |  (dab blitting)      |  curve fitting
    v                     v
RasterStroke          VectorStroke  (with BezierPath)
    |                     |
    +--------┬------------+
             |
             v
        RasterLayer / VectorLayer
             |
             v
        GroupLayer (compositing with blend modes)
             |
             v
        Canvas Display (OpenGL texture)
             |
             v
        Main Window (Qt QOpenGLWidget)
```

### Stroke Pipeline Details

1. **Input Capture**: `InputManager` and `TabletHandler` produce `StrokePoint` structs with position, pressure, tilt, rotation, and timestamp.
2. **Brush Engine**: `BrushEngine` buffers points and applies the active `BrushPreset` (size, opacity, dynamics).
3. **Stroke Finalization**: On `endStroke()`, the point buffer is dispatched:
   - **Raster path**: Points are converted to dab stamps, blended into the layer pixel buffer via `RasterEngine`.
   - **Vector path**: Points are fed through `createPathFromPoints()` which simplifies the polyline into a cubic Bezier path (`BezierPath`), wrapped in a `VectorStroke`.
4. **Layer Storage**: The stroke is stored in the active `RasterLayer` or `VectorLayer`.
5. **Compositing**: `GroupLayer` flattens child layers into a final RGBA image using per-layer blend modes and opacity.
6. **Display**: The composited image is uploaded to an OpenGL texture and rendered in `CanvasWidget`.

---

## File Format (.fap)

A `.fap` project is a **directory** (not a single file) containing:

```
project.fap/
├── document.json         # Project metadata and layer tree
└── frames/
    ├── L0_0000.png       # Layer 0, frame 0000
    ├── L0_0001.png       # Layer 0, frame 0001
    ├── L1_0000.png       # Layer 1, frame 0000
    └── ...
```

### `document.json` Schema

```json
{
  "version": 1,
  "canvas_w": 1920,
  "canvas_h": 1080,
  "fps": 24,
  "total_frames": 60,
  "layers": [
    {
      "name": "Background",
      "type": "raster",
      "visible": true,
      "opacity": 1.0,
      "blend_mode": "normal",
      "locked": false,
      "width": 1920,
      "height": 1080
    },
    {
      "name": "Line Art",
      "type": "vector",
      "visible": true,
      "opacity": 1.0,
      "blend_mode": "normal",
      "locked": false
    },
    {
      "name": "Effects",
      "type": "group",
      "visible": true,
      "opacity": 1.0,
      "blend_mode": "normal",
      "locked": false,
      "children": [ ... ]
    }
  ]
}
```

**Layer types**: `raster`, `vector`, `group`, `audio`, `camera`
**Blend modes**: `normal`, `multiply`, `screen`, `overlay`, `add`, `subtract`, `darken`, `lighten`, `color_burn`, `color_dodge`, `soft_light`, `hard_light`

---

## Build System (CMake)

The project uses CMake 3.20+ with the following structure:

- **Root `CMakeLists.txt`**: Defines the project, finds dependencies (Qt6, GTest), collects source files per module, builds the main executable (`free-animation-power`), and optionally builds the test executable (`fap-tests`).
- **Source grouping**: Sources are organized into `CORE_SOURCES`, `ENGINE_SOURCES`, `UI_SOURCES`, `IO_SOURCES`, `PLATFORM_SOURCES`.
- **Include path**: `src/` is added as a private include directory, allowing includes like `"core/document.hpp"`.
- **CMake options**:
  - `FAP_BUILD_TESTS` (ON/OFF) — enables test build with GoogleTest
  - `FAP_RENDERER_SKIA` (ON/OFF) — enables Skia graphics backend
  - `FAP_RENDERER_OPENGL` (ON/OFF) — enables OpenGL backend

### Test Build

Tests are built as a separate executable `fap-tests` using `GTest::gtest_main` and linked against `Qt6::Core` and `Qt6::Gui`. Test discovery is done via `gtest_discover_tests()`.

Test files (in `tests/`):
- `test_document.cpp` — Document, Timeline, Layer tests
- `test_timeline.cpp` — Timeline-specific tests
- `test_brush_engine.cpp` — Brush engine and preset tests
- `test_bezier.cpp` — Bezier path geometry tests
- `test_file_format.cpp` — Save/load round-trip tests

**Note**: For tests to link correctly, all relevant `.cpp` implementation files from `src/` must be compiled and linked into the test executable. See `docs/build-instructions.md` for details on ensuring proper linkage.
