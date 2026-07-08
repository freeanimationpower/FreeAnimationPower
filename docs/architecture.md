# Free Animation Power — Architecture Document

## Overview

Free Animation Power is a hybrid 2D animation application that combines raster and vector drawing engines into a single unified toolset. It is designed for traditional frame-by-frame animation, digital painting, and vector illustration workflows. The application targets desktop platforms (Windows, macOS, Linux) and is built with C++20, Qt 6, and CMake.

The core philosophy is a **hybrid engine architecture**: every brush stroke can be rendered as pixels (raster), as a vector path, or as both simultaneously. This allows artists to use the strengths of each pipeline without switching tools.

---

## Layer Architecture

```
+------------------------------------------------------------------+
|                          UI Layer (Qt 6)                         |
|  main_window_v2 | canvas_widget_v2 | timeline_panel_v2 | toolbox_panel_v2  |
|  color_panel_v2 | layer_panel_v2   | property_editor_v2 | audio_track_widget|
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
| `document.hpp` / `document.cpp` | `Document` holds canvas dimensions, sequence list, modification state, and audio track metadata. Delegates layer/frame operations to active `Sequence`. |
| `sequence.hpp` / `sequence.cpp` | `Sequence` manages per-sequence frames, playback state, looping, Work Area boundaries, duration frames, per-layer keyframe tracking, and per-sequence undo manager. |
| `layer.hpp` / `layer.cpp` | Layer hierarchy: abstract `Layer` base, `RasterLayer` (with pixel dimensions), `VectorLayer` (stroke collection), and `GroupLayer` (composite with children). |
| `stroke.hpp` / `stroke.cpp` | Stroke data container for brush input points. |
| `canvas.hpp` / `canvas.cpp` | `Canvas` manages the viewport transform: zoom, pan, rotation, and horizontal/vertical flip. |
| `tool_state.hpp` / `tool_state.cpp` | `ToolState` (QObject) centralizes 37 brush/tool/onion-skin properties with change signals. |
| `app_state.hpp` / `app_state.cpp` | `AppState` (QObject) is the central state hub owning Document, ToolState, and all engine subsystems. |

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

### `src/ui_v2/` — User Interface (Qt 6 Widgets)

| File | Description |
|------|-------------|
| `main_window_v2.hpp` / `.cpp` | Application main window with toolbar, dock widgets, dark theme, and centralized signal wiring. |
| `canvas_widget_v2.hpp` / `.cpp` | Central drawing surface with 4-buffer CPU rendering pipeline and 16-tool support. |
| `timeline_panel_v2.hpp` / `.cpp` | Multi-sequence timeline with work area, FPS control, audio tracks, and waveform display. |
| `toolbox_panel_v2.hpp` / `.cpp` | 11-tool palette, color swatch, onion skin settings, canvas resize controls. |
| `color_panel_v2.hpp` / `.cpp` | Color swatch with QColorDialog integration and 9-circle MRU palette. |
| `layer_panel_v2.hpp` / `.cpp` | Layer list with visibility/lock per row, blend mode combo, reorder/duplicate/delete. |
| `property_editor_v2.hpp` / `.cpp` | Context-sensitive editor: brush controls, color picker, fill options, text font controls. |
| `audio_track_widget.hpp` / `.cpp` | Audio track row with waveform visualization, QMediaPlayer, mute/volume controls. |
| `layer_lock_button.hpp` / `.cpp` | Reusable 22x22 lock/unlock toggle button used in layer panel and timeline tracks. |
| `audio_track_widget.hpp` / `.cpp` | Audio track row with waveform visualization, QMediaPlayer, mute/volume controls. |

### `src/io/` — Input/Output

| File | Description |
|------|-------------|
| `document_manager.cpp` | Primary `.fap` format (binary ZIP via miniz). Atomic save/load with pixel deduplication, audio embedding, multi-sequence support. |
| `file_format.cpp` | Legacy directory-based `.fap` format (v1/v2/v3) for backward compatibility. |
| `document_io.cpp` | High-level document save/open with format detection. |
| `video_export.cpp` | Export animation to MP4/GIF/PNG sequence using FFmpeg. |
| `svg_export.cpp` | Export frames to SVG format (single frame and multi-frame). |
| `serialization_common.cpp` | Shared enum/string conversion helpers for layer types and blend modes. |

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
        CanvasWidgetV2 (4-buffer CPU pipeline)
              |
              v
        MainWindowV2 (Qt QWidget, dark theme)
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

## File Format (.fap v2)

A `.fap` project is a **binary ZIP** (renamed `.zip` file). Managed by `DocumentManager` in `src/io/document_manager.cpp`.

```
project.fap (ZIP renamed)
├── manifest.json       — version, canvas size, active_sequence, viewport (zoom/offset)
├── timeline.json       — per-sequence: frames[], layers[], audio[] array
├── audio/track_N.ext   — embedded audio files
└── layers/S{seq}/frame_{f}/layer_{ll}.png + .json
```

Key features:
- **Atomic save**: write to .tmp, rename on success
- **Pixel deduplication**: shared `pixelBuffer_` pointers detected via `unordered_set`
- **ViewState**: zoom, offsetX, offsetY persisted in manifest
- **11 metadata fields** per sequence: name, fps, totalFrames, visible, opacity, locked, workAreaStart/End, durationFrames, looping, currentFrame
- **Layer metadata**: origin_x/y, hasContent, opacity, blendMode, locked
- **Audio embedding**: audio files stored as ZIP entries, extracted to temp on load
- **Backward compatible**: legacy directory format v1/v2/v3 still loadable via `file_format.cpp`
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

### Test Build

Tests are built as a separate executable `fap-tests` using `GTest::gtest_main`. Test discovery via `gtest_discover_tests()`. 154 tests across 17 files.

Test files (in `tests/`):
- `test_document.cpp` — Document, Sequence, Layer tests
- `test_layer.cpp` — Raster/Vector layer pixel operations
- `test_compositor.cpp` — Layer compositing, blend modes, opacity
- `test_undo.cpp` — Undo/redo, max entries, UID preservation
- `test_brush_engine.cpp` — Brush engine presets and stroke lifecycle
- `test_bezier.cpp` — Bezier path geometry, flattening
- `test_file_format.cpp` — Save/load round-trip
- `test_audio_engine.cpp` — Audio tracks, keyframes, time conversion
- `test_deformation.cpp` — Deformation mesh and engine
- `test_node_graph.cpp` — Node graph DAG, blend/filter/transform nodes
- `test_ruler_guide.cpp` — Linear/Radial/Mirror/Perspective snap
- `test_tween_engine.cpp` — Easing functions, keyframe interpolation
- `test_pencil_retouch.cpp` — Pencil retouch modes and brush
- `test_abr_importer.cpp` — ABR brush file detection and load
- `test_layer_memory.cpp` — UID, epoch, CoW, pointer stability
- `test_timeline.cpp` — Playback state, frame navigation
- `test_memory_stress.cpp` — 4K layer stress, undo stack stress

**Note**: For tests to link correctly, all relevant `.cpp` implementation files from `src/` must be compiled and linked into the test executable. See `docs/build-instructions.md` for details on ensuring proper linkage.
