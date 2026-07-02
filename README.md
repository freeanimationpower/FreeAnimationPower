# Free Animation Power

2D Animation Software — Hybrid Vector + Raster Engine. Built with C++20 and Qt 6.

## Overview

Free Animation Power is a professional 2D animation tool combining bitmap (raster) and vector drawing capabilities in a single application. It targets animators who need both traditional frame-by-frame (like TVPaint) and modern cut-out workflows (like Harmony).

**Key philosophy**: Every brush stroke can be rendered as pixels (raster), as a vector path, or as both simultaneously — allowing artists to use the strengths of each pipeline without switching tools.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                        UI Layer (Qt 6)                           │
│  MainWindowV2  CanvasWidgetV2  TimelinePanelV2  ToolboxPanelV2  │
│  ColorPanelV2  LayerPanelV2    PropertyEditorV2                 │
└────────────────────────────┬─────────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────────┐
│                      Engine Layer                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ brush/        │  │ raster/       │  │ vector/              │   │
│  │ BrushEngine   │  │ RasterEngine  │  │ VectorEngine         │   │
│  │ BrushPreset   │  │ RasterStroke  │  │ BezierPath           │   │
│  │ PaperTexture  │  │               │  │ VectorStroke         │   │
│  └──────────────┘  └──────────────┘  └──────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ animation/  TimelineEngine  OnionSkin  Playback  Tween   │   │
│  │ compositor/ NodeGraph  Compositor                         │   │
│  │ deformation/ DeformationMesh  DeformationEngine           │   │
│  └──────────────────────────────────────────────────────────┘   │
└────────────────────────────┬─────────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────────┐
│                     Core Data Model                               │
│  Document  Timeline  Layer (Raster/Vector/Group)  Canvas         │
│  Project  Stroke  UndoManager  ToolState  AppState               │
│  Types (Vec2, Color, Rect, StrokePoint, BlendMode, LayerUid)     │
└────────────────────────────┬─────────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────────┐
│                       Platform Layer                              │
│  IO: document_io  video_export  file_format (.fap)               │
│  Input: tablet_handler  input_manager                            │
└──────────────────────────────────────────────────────────────────┘
```

### Directory Map

| Directory | Purpose |
|-----------|---------|
| `src/core/` | Data model — Document, Layer, Timeline, Stroke, Canvas, Undo, ToolState |
| `src/engine/raster/` | Bitmap pixel engine — stamp blitting, alpha compositing |
| `src/engine/vector/` | Bezier path engine — curve evaluation, flattening, path construction |
| `src/engine/brush/` | Brush engine — presets, dynamics, paper texture, ABR import |
| `src/engine/animation/` | Timeline playback, onion skinning, audio, tween |
| `src/engine/compositor/` | Layer compositing with blend modes, node graph |
| `src/engine/deformation/` | Mesh deformation for puppet animation |
| `src/ui_v2/` | Qt 6 widgets — Canvas, panels, MainWindow (active version) |
| `src/ui/` | Legacy V1 UI (not compiled in current build) |
| `src/io/` | File format (.fap), video export, document I/O |
| `src/platform/` | Input handling, tablet (Wacom) support |
| `docs/` | Architecture docs, build instructions, handoffs |
| `tests/` | GoogleTest suite (139 tests) |

### Layer Types

- **RasterLayer**: Pixel buffer with per-frame PNG storage
- **VectorLayer**: Bezier stroke collection
- **GroupLayer**: Recursive layer composition with blend modes
- **AudioLayer**: Audio clip timeline

### File Format (.fap)

Directory-based format:
```
project.fap/
├── document.json    # Metadata + layer tree (JSON)
└── frames/
    ├── L0_0000.png  # Layer 0, frame 0
    ├── L0_0001.png  # Layer 0, frame 1
    └── ...
```

Blend modes: normal, multiply, screen, overlay, add, subtract, darken, lighten, color_burn, color_dodge, soft_light, hard_light.

---

## Current State (v2.0.0)

### What Works
- All 11 tools: Brush, Eraser, Pick Color, Fill, Text, Line, Rectangle, Ellipse, Move, Select, Hand
- Pressure-sensitive brushes with tablet support (Wacom)
- Multi-layer document with blend modes and opacity
- Timeline with frame navigation, playback, onion skinning
- Undo/Redo with full pixel restoration
- Color picker with RGBA/Hex, recent colors palette
- Canvas view: zoom, pan, fit, flip horizontal, rotate, grid
- Video export (MP4 via FFmpeg), GIF export
- File save/open (.fap format)
- 139 unit tests, all passing
- Keyboard shortcuts for all tools and operations

### UI Layout
```
┌────────────────────────┬──────────────────────┬──────────────────┐
│ TopBar                 │                      │  Layers Panel    │
│ [New][Open][Save] |    │    CanvasWidget      │  - Layer list    │
│ [Export] | [View] |    │    (central widget)   │  - Visibility    │
│ [Undo][Redo] |         │                      │  - Opacity       │
│ 🧅[Onion Skin] |       │    Drawing canvas    │  - Blend modes   │
│ 📐[Canvas Size] | [?]  │                      │  - Add/Dup/Del   │
├────────────────────────┤                      ├──────────────────┤
│  ToolboxPanel          │                      │  Color Panel     │
│  - 11 tool buttons     │                      │  - RGBA spinners │
│  - Color swatch        │                      │  - Hex input     │
│  - Brush settings      │                      │  - Recent colors │
│  - Onion Skin          │                      │                  │
│  - Canvas Size         │                      │                  │
├────────────────────────┴──────────────────────┴──────────────────┤
│  Timeline Panel  [◀][▶][■]  Frame: 1/24  24fps  [+][++][−]    │
│  [thumb][thumb][thumb][thumb][thumb]...                          │
└──────────────────────────────────────────────────────────────────┘
```

---

## Problems Solved — Engineering History

### 1. Brush Undo Ghost Outlines (commit `b1f0af1` → `0f92cf1`)

**Problem**: After Ctrl+Z on a brush stroke, a ghost outline of the stroke remained visible. The undo didn't fully restore pre-stroke pixels.

**Root cause**: The "before" snapshot was captured from the live layer buffer *after* adjacent stamps had already been blended, contaminating the undo image with partial stroke data.

**Solution**:
1. Added `cleanLayerCopy_` — a clean copy of the layer captured at stroke start, before any painting.
2. `growBeforeSnapshot()` now reads from `cleanLayerCopy_` instead of the live layer.
3. This prevents adjacent stamp paint from leaking into the undo "before" image.

**Files**: `canvas_widget_v2.cpp`, `undo_manager.cpp`

---

### 2. Ghost Rectangle Artifact on Brush Strokes (commit `8ab7b21`)

**Problem**: A visible rectangular artifact appeared at the `dirtyRect` boundary after committing a brush stroke. The artifact showed as a faint rectangle on the canvas.

**Root cause**: `UndoManager::pushCommand()` called `cmd->redo()` immediately after storing the command. `PaintCommandV2::applySnapshot()` (called by redo) wrote the after-snapshot back to the layer buffer, introducing a visible seam at the dirty rectangle boundary.

**Solution**:
1. Added `UndoManager::pushApplied()` — stores the command without calling `redo()`, for actions already applied to the buffer.
2. Updated `commitStroke()`, `commitFloatingSelection()`, and `commitTransform()` to use `pushApplied()`.

**Additional fixes in this commit**:
- `layer.cpp`: Store premultiplied alpha, un-premultiply on read (matching `Format_ARGB32_Premultiplied` display).
- `pencil_retouch.cpp`: Fixed R/B channel swap in `repaintPixel`, scale RGB with alpha.
- `canvas_widget_v2.cpp`: Convert layer QImage to straight ARGB32 before `drawImage`, match V1 pixel-snapping.

**Files**: `undo_manager.cpp`, `canvas_widget_v2.cpp`, `layer.cpp`, `pencil_retouch.cpp`

---

### 3. Aggressive Buffer Expansion (commit `0aa9e07`)

**Problem**: After a few move operations, the layer buffer ballooned to 10000×10000 pixels due to `ensureContains()` always adding a growth pad (512px) and guard band (2px).

**Root cause**: `ensureContains()` unconditionally added `kGrowthPad` (512) and `kGuardBand` (2) to every buffer resize, causing geometric growth on repeated moves.

**Solution**: Added optional `pad` parameter to `ensureContains()`. When `pad=false`, skips the growth expansions for exact-fit resizes. Used in `commitMove()` for precise expansion. All other callers keep default `pad=true`.

**Files**: `layer.cpp`, `canvas_widget_v2.cpp`

---

### 4. Content Clipping on Move (commit `c72a85a`)

**Problem**: Content was silently clipped when moved beyond the original canvas boundary.

**Solution**: `commitMove()` now calls `ensureContains()` to expand the layer buffer before shifting pixels, preserving content moved beyond canvas bounds. Matches Synfig's auto-expansion pattern.

**Files**: `canvas_widget_v2.cpp`, `layer.cpp`

---

### 5. Qt Raster Tile Seams (commit `cced79d`)

**Problem**: Visible seams appeared between Qt raster tiles when rendering the canvas, especially at certain zoom levels.

**Root cause**: Qt's internal tile-based rendering of QImage in `drawImage` introduced 1px seams due to antialiasing on tile boundaries and sub-pixel matrix offsets.

**Solution**:
1. Disabled antialiasing on `drawImage` calls.
2. Snapped matrix translation to integer device pixels.
3. Added 1px padded/clamped image cache per layer (epoch-invalidated).
4. Zero-copy `QImage` with `shared_ptr` lifetime for `PixelBuffer` via `wrapRasterLayer()`.

**Additional fixes**: Brush stamp rounding (`std::round`), paper texture bilinear wrap fix, memory purge menu, frame thumbnail off-by-one fix.

**Files**: `canvas_widget_v2.cpp`, `layer.cpp`, `raster_stroke.cpp`, `paper_texture.cpp`

---

### 6. Undo Capture Rect Mismatch (commit `06ceb10`)

**Problem**: `captureRect()` / `captureLayerRect()` clamped to layer bounds, returning smaller images than requested. The undo command stored the unclamped rect, causing pixel offset mismatches on restore — leaving ghost outlines after undo.

**Solution**: Now returns exact requested rect with zero-fill for out-of-bounds pixels, matching Photoshop/Illustrator snapshot semantics.

**Files**: `canvas_widget_v2.cpp`

---

### 7. Guard Band & Alpha Compositing (commit `d98eb53`)

**Problem**: Brush strokes had visible artifacts at edges due to missing transparency padding and incorrect alpha blending.

**Solution**:
1. `ensureContains()` adds `kGuardBand=2` transparent padding around strokes.
2. `paintLiveStroke()` renders to offscreen transparent QImage before blitting (single-click draws padded circle stamp).
3. `commitStroke()` uses small QImage stamp with `kStampPad=2` and manual premultiplied alpha compositing via `blendStrokeToLayerAt()`.
4. `blendStrokeToLayerAt()`: premultiplied alpha compositing (src OVER dst) with `sa<4` threshold to skip near-transparent pixels.
5. `writeLayerPixels()` / `writeRasterRect()`: sanitize alpha=0 pixels to `0x00000000`.

**Files**: `layer.cpp`, `canvas_widget_v2.cpp`

---

### 8. Keyboard Shortcuts & Selection Ops (commit `3cc22e2`)

**Problem**: Ctrl+Z/Y keyboard shortcuts for undo/redo not working. Selection tools missing copy/cut/paste/delete operations.

**Solution**: Fixed all keyboard shortcuts in `keyPressEvent`, added selection clipboard operations (Copy Ctrl+C, Cut Ctrl+X, Paste Ctrl+V, Delete, Select All Ctrl+A, Deselect Ctrl+D).

**Files**: `canvas_widget_v2.cpp`

---

### 9. Tool Implementation in V2 Canvas (commit `d010f63`)

**Problem**: V2 canvas only had brush tool. All other tools were declared but empty stubs.

**Solution**: Implemented all 11 tools in V2 canvas: Eraser, Pick Color, Fill, Text, Line, Rectangle, Ellipse, Move, Select, Hand. Added dynamic color palette generation, fixed selection rectangle rendering bugs.

**Files**: `canvas_widget_v2.cpp`, `color_panel_v2.cpp`

---

### 10. Brush Controls Unification (commit `9391248`)

**Problem**: Brush controls (size, opacity, hardness, shape) were scattered across ToolboxPanel and not connected to the canvas.

**Solution**: Unified all brush controls in `PropertyEditorV2`. Connected brush size slider, blend mode, and other settings to the canvas. Timeline thumbnails now render with white background.

**Files**: `property_editor_v2.cpp`, `canvas_widget_v2.cpp`, `timeline_panel_v2.cpp`

---

### 11. Text Tool Redesign — Font Picker + Character Panel + Editable Text (commit `this`)

**Problem**: The text tool had multiple critical issues:
- Only Arial loaded; other fonts didn't apply due to style combo populated after font apply
- Font combo showed bitmap fonts causing DirectWrite warnings ("Terminal", "Terminal Greek")
- Text position was offset by layer origin on commit
- Caret position was wrong for centered/right-aligned text
- Clicking UI panels auto-committed text (focusOutEvent)
- Text typed in property panel was wiped on canvas click
- No way to re-edit text after committing
- `ToolState::resetToDefaults()` never called on startup

**Solution**:

**1. Custom font picker** (replaced QFontComboBox):
- QPushButton + QMenu popup anchored to the button
- Search bar with real-time filtering
- Each item renders the user's **actual text** in that font at 14pt
- Font cache enumerated once via `QFontDatabase`, filtered by `isSmoothlyScalable()`
- No DirectWrite warnings, instant opening after first load

**2. Character panel** (Photoshop-inspired):
```
CHARACTER
Font: [Arial                    ▼]  ← click opens inline font picker
Style:[▼ Regular / Bold / Italic  ]
Sz:[24] Ld:[0%] Tr:[0%]           ← Size, Leading, Tracking
Color: [●]                         ← current color swatch
[B][I][U][S]                       ← Bold, Italic, Underline, Strikethrough
AA: [▼ Smooth / Sharp / None]     ← Anti-aliasing mode
Align: [◀][▶◀][▶]                 ← Left, Center, Right
```

**3. Editable text after commit**:
- Each committed text stored as `TextEntry {pos, text, font, color}` per frame
- Click near existing text (12px/24px tolerance) to reload and re-edit
- Visual indicator: dotted underline under each entry when Text tool is active

**4. Typography controls** added to `ToolState`:
- `textLeading` (line spacing %), `textTracking` (letter spacing %)
- `textAlignment` (0=Left, 1=Center, 2=Right)
- `textAntiAliasing`, `textUnderline`, `textStrikethrough`
- Multi-line text with configurable alignment, leading, tracking, anti-aliasing

**5. Bugs fixed**:
- `focusOutEvent`: no longer commits when focus goes to app panels (check `isAncestorOf`)
- `doText()`: subtracts `layer->originX()/originY()` for correct canvas position
- Caret aligned per alignment mode (left: after text, center: center+half-width, right: at origin)
- Text no longer wiped on canvas click; cleared only after commit
- `ToolState::resetToDefaults()` called in constructor and `resetDocument()`
- `tween_engine::interpolatePaths()`: guard against `.back()` on empty segments vector

**6. Font loading**:
- JetBrainsMono bundled in `resources/fonts/` via QRC, loaded at startup
- `QFontDatabase::addApplicationFont()` in `main.cpp`

**Files**: `tool_state.hpp/cpp`, `app_state.cpp`, `property_editor_v2.hpp/cpp`, `canvas_widget_v2.hpp/cpp`, `main.cpp`, `resources.qrc`, `tween_engine.cpp`, `resources/fonts/`

**UX flow**:
```
1. Press T → CHARACTER panel appears
2. Type directly on canvas (keyboard) — text appears with blinking caret
3. Adjust font, style, size, color, alignment in panel — applies live
4. Enter → commits text to raster layer
5. Escape → cancels
6. Click near existing text → reloads for re-editing
```

---

### 12. Display Pipeline Refactor — 4-Buffer CPU Architecture (Jul 2026)

**Problem**: The previous attempt to fix GPU rendering artifacts (`QPainter` on `QOpenGLWidget`) created an irresolvable trade-off:
- Separating dabs from background = correct frame data but white-line display artifacts
- Baking everything into a single opaque image = no artifacts but corrupted frame data (onion skin + composite layers baked into active layer on mouse release)

After 41 commits and a full rollback to commit `1f5f4dc`, a different approach was needed.

**Root cause**: `QPainter` on `QOpenGLWidget` has premultiplied alpha compositing bugs. GPU workarounds failed. The CPU `QWidget` path was the correct approach, but needed architectural restructuring.

**Solution**: 4-buffer CPU rendering pipeline with strict **DATA** vs **DISPLAY** separation:

```
                    ┌──────────────────────────┐
                    │      DATA DOMAIN          │
                    │  (saved to .fap file)     │
                    ├──────────────────────────┤
                    │ pixelBuffer_ (per layer)  │◄── source of truth
                    │ strokeBuffer_ (isolated)  │◄── current stroke dabs
                    └──────────┬───────────────┘
                               │ commitStroke() composites
                               ▼
                    ┌──────────────────────────┐
                    │     DISPLAY DOMAIN        │
                    │  (what the user sees)     │
                    ├──────────────────────────┤
                    │ backgroundCache_          │◄── white + onion + layers
                    │ paintEvent                │◄── cache + stroke + UI
                    └──────────────────────────┘
```

**Key invariants**:
| Rule | How |
|------|-----|
| `mouseReleaseEvent` never reads from display buffers | Only reads `strokeBuffer_` (transparent) and `pixelBuffer_` (transparent layer data) |
| Dabs isolated during stroke | `drawBrushStamp()` writes to `strokeBuffer_` only; composited onto `pixelBuffer_` on commit |
| No full rebuild for offset layers | `buildBackgroundCache(rect)` uses `rect.translated(-originX, -originY)` per layer |
| Dynamic feathering padding | `brushSize / 2 + 1` pixels — covers entire dab diameter including soft edges |

**Removed from codebase**: `activeDirtyRect_`, `cleanLayerCopy_`, `growBeforeSnapshot()` — all made obsolete by stroke isolation.

**New methods**: `buildBackgroundCache(rect)`, `invalidateBackgroundCache()`.

**Performance**: `paintEvent` draws 1 `backgroundCache_` image instead of iterating N layers with N `drawImage` calls. Dirty rect partial rebuilds limit recomposition to the stroke area on commit.

**Files**: `src/ui_v2/canvas_widget_v2.hpp`, `src/ui_v2/canvas_widget_v2.cpp`  
**Session report**: `docs/session-report-2026-07-02.md`

---

## Build & Run

### Quick Start (Windows)
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\free-animation-2d-style.exe
```

### Run Tests
```powershell
ctest --test-dir build
```
**Status**: 139/139 tests passing.

### Requirements
- CMake 3.20+
- Qt 6.5+ (Core, Gui, Widgets, OpenGL, OpenGLWidgets, Multimedia, Svg)
- FFmpeg (for video export)
- C++20 compiler (MSVC 2022+, GCC 11+, Clang 14+)
- GoogleTest (fetched automatically via CMake FetchContent)

### CMake Options
| Option | Default | Description |
|--------|---------|-------------|
| `FAP_BUILD_TESTS` | ON | Build test suite |
| `CMAKE_BUILD_TYPE` | Release | Debug, Release, RelWithDebInfo |

For detailed build instructions per platform (Linux, macOS), see `docs/build-instructions.md`.

---

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Language | C++20 |
| UI Framework | Qt 6.5+ (Widgets) |
| Build System | CMake 3.20+ |
| Rendering | Qt Raster (QPainter) + OpenGL |
| Testing | GoogleTest (139 tests) |
| Compression | miniz (zlib) |
| Video Export | FFmpeg (MP4, GIF) |
| Platform | Windows (primary), macOS/Linux (target) |

---

## Documentation

- `docs/architecture.md` — Full architecture document with data flow diagrams
- `docs/build-instructions.md` — Detailed build instructions for all platforms
- `docs/handoff-canvas-state-machine.md` — Canvas state machine handoff notes
- `AGENTS.md` — AI agent instructions for code navigation

---

## Author

**Eduardo Fierro Duque**
- Web: [www.fierroduque.com](https://www.fierroduque.com)
- LinkedIn: [linkedin.com/in/eduardofierroduque](https://www.linkedin.com/in/eduardofierroduque/)

## License

Free Animation Power is licensed under the **GNU General Public License v3.0** (GPLv3).

```
Copyright (C) 2026 Eduardo Fierro Duque

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

See [LICENSE](LICENSE) for the full license text.
