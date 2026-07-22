# Free Animation Power — Architecture Document

## Overview

Free Animation Power is a hybrid 2D animation application combining raster and vector drawing engines. It targets traditional frame-by-frame animation, digital painting, and vector illustration. Built with C++20, Qt 6, and CMake for Windows, macOS, and Linux.

## Architecture Stack

| Layer | Components |
|-------|-----------|
| **UI** | MainWindowV2, CanvasWidgetV2, TimelinePanelV2, ToolboxPanelV2, ColorPanelV2, LayerPanelV2, PropertyEditorV2, AudioTrackWidget, VideoTrackWidget, OnionSkinPanel, CanvasSizePanel, BusyOverlay |
| **AppState** | Document, ToolState, AudioEngine, RulerTool, TweenEngine, DeformationEngine, FrameThumbnailCache, PencilRetouchEngine |
| **Engine** | BrushEngine, RasterEngine, VectorEngine, Compositor, NodeGraph, DeformationMesh, AudioEngine, AudioFileDecoder, VideoDecoder, TweenEngine |
| **Core** | Document, Sequence, Layer (Raster/Vector/Group), UndoManager, ToolState, Types |
| **Platform** | DocumentManager (.fap), VideoExport, FileAssociation (Windows) |

## Module Descriptions

### src/core/ — Core Data Model

| File | Description |
|------|-------------|
| `types.hpp/cpp` | Fundamental types: `Color`, `Vec2`, `Rect`, `StrokePoint`, enums (`LayerType`, `BlendMode`, `ToolType`, `BrushMode`) |
| `document.hpp/cpp` | `Document` — canvas dimensions, sequence list, audio/video track metadata, modification state |
| `sequence.hpp/cpp` | `Sequence` — per-sequence frames, playback state, Work Area, markers, LineBoil, undo manager |
| `layer.hpp/cpp` | Layer hierarchy: `RasterLayer` (COW pixel buffer), `VectorLayer` (stroke collection), `GroupLayer` (recursive composite) |
| `undo_manager.hpp/cpp` | `UndoManager` — per-sequence push/undo/redo with depth limit 100, `UndoGroup` batching |
| `tool_state.hpp/cpp` | `ToolState` (QObject) — 37 brush/tool/onion-skin properties with bidirectional Qt signal binding |
| `app_state.hpp/cpp` | `AppState` (QObject) — central state hub owning Document, ToolState, and all engine subsystems |

### src/engine/ — Engine Layer

#### brush/ — Brush Engine

| File | Description |
|------|-------------|
| `brush_engine.hpp/cpp` | `BrushEngine` — stroke state machine |
| `brush_preset.cpp` | Factory: Round Soft, Round Hard, Flat, Calligraphy, Eraser |
| `paper_texture.cpp` | Paper grain simulation |
| `abr_importer.hpp/cpp` | Photoshop .abr import (v1, v2, v6+) |
| `pencil_retouch.hpp/cpp` | `PencilRetouchEngine` — thickness adjustment, smoothing |
| `ruler_guide.hpp/cpp` | `RulerTool` — linear, radial, arc guides + snap |

#### raster/ — Raster Engine

| File | Description |
|------|-------------|
| `raster_engine.hpp/cpp` | `RasterEngine` — stamp blitting, alpha blending |
| `raster_stroke.cpp` | `RasterStroke` — dab series |
| `raster_effect.hpp/cpp` | `applyLineBoil()` — bilinear noise displacement |

#### vector/ — Vector Engine

| File | Description |
|------|-------------|
| `bezier_path.hpp/cpp` | `BezierPath` — cubic Bezier: evaluateAt, tangentAt, flattenPoints |
| `vector_engine.hpp/cpp` | `VectorEngine` — path simplification, stroke management |
| `vector_stroke.cpp` | `VectorStroke` — BezierPath + color, width, opacity |

#### animation/ — Animation

| File | Description |
|------|-------------|
| `audio_engine.hpp/cpp` | `AudioEngine` — QMediaPlayer audio |
| `audio_file_decoder.hpp/cpp` | `decodeAudioFile()` — dr_libs (WAV, MP3, FLAC) |
| `video_decoder.hpp/cpp` | `decodeVideoFrame()`, `probeVideoMetadata()` — FFmpeg, LRU cache 50 |
| `tween_engine.hpp/cpp` | `TweenEngine` — Linear, EaseIn/Out/InOut interpolation |
| `frame_thumbnail.hpp/cpp` | `FrameThumbnailCache` — LRU thumbnails |
| `dr_wav.h`, `dr_mp3.h`, `dr_flac.h` | David Reid native decoders |

#### compositor/ — Compositing

| File | Description |
|------|-------------|
| `compositor.hpp/cpp` | `Compositor` — layer flattening, blend modes (bottom to top) |
| `node_graph.hpp/cpp` | `NodeGraph` — DAG: Input, Blend, Filter, Transform, Output nodes |

#### deformation/ — Deformation

| File | Description |
|------|-------------|
| `deformation_mesh.hpp/cpp` | `DeformationMesh` — control points, bilinear map |
| `deformation_engine.hpp/cpp` | `DeformationEngine` — mesh deformation to layers |

#### common/ — Shared

| File | Description |
|------|-------------|
| `blend_utils.hpp` | `unpackPixel()` / `packPixel()` — ARGB32 conversion, blend pixels |

### src/ui_v2/ — Qt 6 Widgets

| File | Description |
|------|-------------|
| `main_window_v2.hpp/cpp` | Main window: 8 docks, dark theme, toolbar, signal wiring |
| `canvas_widget_v2.hpp/cpp` | Canvas: 4-buffer pipeline, 17 tools, tablet events |
| `timeline_panel_v2.hpp/cpp` | Timeline: multi-seq, WA, markers, FPS, non-destructive rebuild |
| `toolbox_panel_v2.hpp/cpp` | Tool palette: 17 tool buttons (36px) |
| `color_panel_v2.hpp/cpp` | Color swatch + 9-circle MRU |
| `layer_panel_v2.hpp/cpp` | Layer list: visibility, lock, blend, opacity, rename |
| `property_editor_v2.hpp/cpp` | Context editor: brush, color, fill, text |
| `audio_track_widget.hpp/cpp` | Audio: waveform (800 peaks), mute, volume |
| `video_track_widget.hpp/cpp` | Video: thumbnails, opacity, FFmpeg decode |
| `onion_skin_panel.hpp/cpp` | Onion skin: enabled, prev/next count, opacity |
| `canvas_size_panel.hpp/cpp` | Canvas size: width x height + Apply |
| `busy_overlay.hpp/cpp` | Spinner overlay, event filter |
| `layer_lock_button.hpp/cpp` | 22x22 lock/unlock toggle |

### src/io/ — I/O

| File | Description |
|------|-------------|
| `document_manager.hpp/cpp` | .fap v2 format: atomic save, pixel dedup, audio/video embedding |
| `file_format.cpp` | Legacy v1/v2/v3 backward compatibility |
| `video_export.hpp/cpp` | MP4/MOV/WebM/GIF/PNG sequence via FFmpeg |
| `svg_export.hpp/cpp` | SVG single + multi-frame export |
| `serialization_common.hpp/cpp` | Enum/string conversion helpers |

### src/platform/ — Platform

| File | Description |
|------|-------------|
| `file_association.hpp/cpp` | Windows .fap registry association |

---

## Display Pipeline (4-Buffer CPU)

| Buffer | Domain | Contents |
|--------|--------|----------|
| `pixelBuffer_` | DATA | Per-layer pixels, transparent bg, source of truth |
| `strokeBuffer_` | DATA | Isolated current stroke, transparent ARGB32 |
| `backgroundCache_` | DISPLAY | Pre-composited: white bg + onion skin + layers + video |
| `paintEvent` | DISPLAY | Composes cache + stroke overlay + grid + UI overlays |

**Key invariants**:
- `mouseReleaseEvent` NEVER reads from display buffers
- Dabs written to isolated `strokeBuffer_`, composited to `pixelBuffer_` on commit
- `buildBackgroundCache(rect)` supports full and partial (offset-aware dirty rect) rebuilds
- Dynamic padding: `brushSize/2 + 1` to cover dab feathering
- Layers with `originX_`/`originY_` use `rect.translated(-originX, -originY)`
- Video tracks active = partial rebuild escalates to full

---

## Data Flow

1. **Input**: `QTabletEvent` / `QMouseEvent` → `CanvasWidgetV2`
2. **Tool Dispatch**: `ToolState::activeTool()` → tool handler
3. **Brush Engine**: `BrushEngine::beginStroke` → `addPoint x N` → `endStroke`
4. **Stroke Render**:
   - Raster: Points → dab stamps → `strokeBuffer_` → `commitStroke()` → `pixelBuffer_`
   - Vector: Points → `createPathFromPoints()` → `BezierPath` → `VectorStroke`
5. **Compositing**: `Compositor::compositeLayer()` flattens layers (bottom→top, per-layer blend modes + opacity)
6. **Display**: `buildBackgroundCache()` → `paintEvent()` → CPU QPainter

---

## File Format (.fap v2)

Binary ZIP renamed to `.fap`. Managed by `DocumentManager`.

- `manifest.json` — version, canvas size, active_sequence, viewport, markers, LineBoil
- `timeline.json` — per-sequence: frames[], layers[], audio[], video[]
- `audio/track_N.ext` — embedded audio (WAV/MP3/FLAC)
- `video/track_N.ext` — embedded video (MP4/MOV/WebM)
- `layers/S{seq}/frame_{f}/layer_{ll}.png` + `.json` — pixels + metadata

**Features**: atomic save (.tmp → rename), pixel dedup, viewport persistence, 11 seq metadata fields, audio/video embedding, backward compatible v1/v2/v3.

---

## Build System

- CMake 3.20+, C++20, Qt 6.5+
- Targets: `free-animation-power` (exe), `fap-tests` (160 tests)
- Dependencies: miniz 3.0.2 (FetchContent), GTest 1.14.0 (FetchContent), FFmpeg (external)
- Windows: `version.rc` embeds metadata + icon at compile time, `windeployqt` post-build

### Test Suite (16 files, 160 tests)

| File | Coverage |
|------|----------|
| `test_document.cpp` | Document, Sequence, markers, WA, LineBoil |
| `test_layer.cpp` | RasterLayer pixel ops, clear, ensureContains, hasContent |
| `test_layer_memory.cpp` | UID, buffer epoch, COW, pixel dedup |
| `test_compositor.cpp` | Layer flattening, z-order, blend modes |
| `test_undo.cpp` | Undo/redo, depth limit, multi-seq, UndoGroup |
| `test_brush_engine.cpp` | BrushEngine presets, stroke lifecycle |
| `test_bezier.cpp` | BezierPath geometry, flattenPoints |
| `test_file_format.cpp` | .fap save/load, binary ZIP, multi-seq |
| `test_audio_engine.cpp` | AudioTrack: mute/solo, volume, clips |
| `test_deformation.cpp` | DeformationMesh bounds, bilinear |
| `test_node_graph.cpp` | NodeGraph DAG, evaluate, dangling ptr |
| `test_ruler_guide.cpp` | RulerTool linear/radial guides, snap |
| `test_tween_engine.cpp` | Easing curves, interpolation |
| `test_pencil_retouch.cpp` | Modes, strength, stroke detection |
| `test_abr_importer.cpp` | .abr file detection, brush tip conversion |
| `test_memory_stress.cpp` | 2000 layers, 500-frame loop, dedup, save/load |
