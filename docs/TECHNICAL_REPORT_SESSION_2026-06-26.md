# Technical Report — Free Animation Power

**Session Date**: 2026-06-26
**Repository**: `github.com/eduardofierroduque-sudo/FreeAnimationPower` (private)
**Base Commit**: `0aa9e07` — fix: add pad parameter to ensureContains to prevent aggressive buffer expansion

---

## Session Summary

This session focused on three major feature implementations and several UX improvements for the 2D animation application. All changes maintain backward compatibility with the existing codebase (139 unit tests passing, zero regressions).

---

## Implemented Features

### 1. Pick Color (Eyedropper) Visual Feedback

**Problem**: The Pick Color tool (`I` key) worked correctly — sampled pixel color from the canvas and updated the toolbox/color panel — but provided zero visual feedback. No cursor change, no preview of what was being sampled. Users couldn't tell where the eyedropper was pointing.

**Solution** (3 files modified):

| File | Change |
|------|--------|
| `canvas_widget_v2.hpp` | Added `lastSamplePos_` (QPointF) and `sampledColor_` (QColor) members |
| `canvas_widget_v2.cpp:setTool()` | Sets `Qt::CrossCursor` when ColorPicker active, restores `Qt::ArrowCursor` otherwise |
| `canvas_widget_v2.cpp:mousePressEvent/mouseMoveEvent` | Stores sampled position and color, triggers `update()` |
| `canvas_widget_v2.cpp:paintEvent` | Draws 18px color preview circle near cursor: outer black ring → white ring → sampled color fill. Rendered in widget coordinates (outside canvas transform) for consistent visibility |

**Technical details**:
- Preview circle is offset 20px up-right from cursor
- Uses p.restore()/p.save() to switch between canvas and widget coordinate systems
- `canvasToWidget()` converts canvas coords to widget coords for circle placement

---

### 2. Layer Rename on Double-Click

**Problem**: The V2 layer panel had no rename functionality. The V1 had it via `QLineEdit` overlay on double-click, but V2 never implemented it.

**Solution** (2 files modified):

| File | Change |
|------|--------|
| `layer_panel_v2.hpp` | Added `startRename(int index, QWidget* row)` private method |
| `layer_panel_v2.cpp:eventFilter()` | Added `MouseButtonDblClick` event detection — excludes clicks on QPushButton children |
| `layer_panel_v2.cpp:startRename()` | Inserts `QLineEdit` at label position in the row's `QHBoxLayout`, hides original `QLabel`, pre-fills with current name, selects all text, applies accent border (`#FF6B4A`) |
| `layer_panel_v2.cpp:commitRename()` (lambda) | On `returnPressed` / `editingFinished`: calls `layer->setName()`, restores label, marks document modified, refreshes list, re-emits `layerChanged` |

**Technical details**:
- `QLineEdit` styled with theme colors: `#1E2130` background, `#FF6B4A` focus border
- Uses `QObject::connect` inside `startRename()` with lambdas for commit logic
- `editingFinished` handles focus-loss commit (clicking elsewhere)

---

### 3. Property Editor Color Variations Auto-Refresh

**Problem**: When using the Pick Color tool, the Property Editor's color variation grid (9 swatches with Monochromatic/Analogous/Triadic modes) stayed frozen on the old color. The user had to manually open/close the variation panel or switch tools to trigger a refresh.

**Root cause**: `MainWindowV2::setupConnections()` connected `CanvasWidgetV2::colorPicked` to update `toolbox_panel_` and `color_panel_`, but never called `property_editor_->updateColorVariations()`.

**Solution** (2 files modified):

| File | Change |
|------|--------|
| `property_editor_v2.hpp` | Moved `updateColorVariations()` from `private` to `public` |
| `main_window_v2.cpp:setupConnections()` | Added `property_editor_->updateColorVariations()` call in the `colorPicked` handler |

**Signal flow** (synchronous, no race conditions):
```
Canvas click → setPrimaryColor() + setSampledColor() → emit colorPicked(qc)
  → toolbox updated ✓
  → color panel updated ✓
  → property editor variations updated ✓  [NEW]
```

---

### 4. Text Tool Redesign — No Dialogs, In-Canvas Editing

**Problem**: The Text tool (`T` key) required two modal dialogs (`QInputDialog` for text + `QFontDialog` for font) before placing text. No preview, no live editing, jarring UX.

**Solution** (5 files modified, 3 new ToolState properties):

#### Architecture Changes

**`ToolState`** (core data model):
- New property: `textString` (QString) — the text content
- New property: `textFont` (QFont) — font family, size, weight, style
- Default font: Arial 24pt
- Signals: `textStringChanged`, `textFontChanged`
- Both reset to defaults in `resetToDefaults()`

**`PropertyEditorV2`** (UI panel):
- New `textGroup_` widget with:
  - `QFontComboBox` — auto-loads ALL installed system fonts (native Qt behavior)
  - `QSpinBox` for font size (8-200px, step 1)
  - `QCheckBox` for Bold (label: "B")
  - `QCheckBox` for Italic (label: "I")
  - `QPlainTextEdit` for multi-line text input (70px height, placeholder "Type your text here...")
  - Hint label: "Click canvas to place text"
- Methods: `showTextControls()`, `syncTextFromState()`
- Full signal wiring: all UI changes → `ToolState::setTextFont()` / `setTextString()` for single source of truth

**`CanvasWidgetV2`** (canvas interaction):
- **In-canvas text editing mode** (`editingText_` state):
  - `mousePressEvent`: first click enters edit mode at cursor position; subsequent click commits current text and starts new edit
  - `keyPressEvent` (early return at top): Enter/Escape/Backspace/text capture when editing
  - `paintEvent`: renders live text preview with `QPainter::drawText()` at `textEditPos_` + blinking caret (500ms QTimer)
  - `focusOutEvent`: auto-commits on focus loss (click outside canvas)
- `commitTextEdit()`: if text non-empty, calls `doText()` which rasterizes to layer via existing pipeline
- `doText()` simplified: reads `textString`/`textFont` from `ToolState` (zero dialogs)
- Removed: `QInputDialog`, `QFontDialog` includes, `textFont_` member

**Technical details**:
- Caret blink timer: `QTimer` with 500ms interval, toggles `caretVisible_` boolean
- Caret position calculated with `QFontMetrics::horizontalAdvance()` for precise placement after text
- Caret line: 1.5px width at zoom level, drawn from -20% ascent to +110% descent
- Multi-click support: clicking again commits current text, starts new edit at new position

---

## Files Modified Summary

| # | File | Lines +/- | Purpose |
|---|------|-----------|---------|
| 1 | `src/core/tool_state.hpp` | +11 | `textString` + `textFont` Q_PROPERTY, methods, signals, members |
| 2 | `src/core/tool_state.cpp` | +21 | Getters, setters with signal emission, reset defaults |
| 3 | `src/ui_v2/canvas_widget_v2.hpp` | +9 | In-canvas text editing members, color sampling members, `focusOutEvent` |
| 4 | `src/ui_v2/canvas_widget_v2.cpp` | +145/-15 | Pick color cursor+preview, text in-canvas editing, caret rendering |
| 5 | `src/ui_v2/layer_panel_v2.hpp` | +1 | `startRename()` declaration |
| 6 | `src/ui_v2/layer_panel_v2.cpp` | +56 | Double-click rename with inline QLineEdit |
| 7 | `src/ui_v2/main_window_v2.cpp` | +1 | `property_editor_->updateColorVariations()` on colorPicked |
| 8 | `src/ui_v2/property_editor_v2.hpp` | +14 | Text group members, `showTextControls()`, `syncTextFromState()` |
| 9 | `src/ui_v2/property_editor_v2.cpp` | +157 | Text group UI creation, signal wiring, group visibility management |

**Total**: 9 files, +400/-15 lines, 0 test regressions (139/139 passing).

---

## Current Architecture State (Post-Session)

### UI Layout
```
┌───────────────────────────┬──────────────────────┬──────────────────────┐
│ TopBar                    │                      │  Layers Panel        │
│ [New][Open][Save] |       │    CanvasWidget      │  - Double-click →    │
│ [Export] | [View] |       │    (central)         │    rename            │
│ [Undo][Redo] |            │                      │  - Visibility/lock   │
│ 🧅[Onion] | 📐[Canvas] |  │  - Crosshair cursor  │  - Blend modes       │
│ [?]                        │    on pick color     │  - Add/Dup/Move/Del  │
├───────────────────────────┤  - Color preview ○   ├──────────────────────┤
│  ToolboxPanel (180px)     │    on pick color     │  Color Panel         │
│  - 11 tool buttons        │  - Live text edit    │                      │
│  - Color swatch           │    with blinking     │                      │
│  - Brush settings         │    caret             │                      │
│  - Onion Skin             │                      │                      │
├───────────────────────────┴──────────────────────┴──────────────────────┤
│  Timeline Panel                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key Files by Concern

**Data Model** (`src/core/`):
- `tool_state.hpp/cpp` — Central state: tool, colors, brush settings, onion, canvas, text, fill, line style
- `types.hpp` — ToolType enum (Brush=0 through Hand=10), Vec2, Color, BlendMode, LayerType
- `layer.hpp/cpp` — Layer hierarchy: RasterLayer, VectorLayer, GroupLayer, with COW, UID, epochs
- `undo_manager.hpp/cpp` — Command pattern undo: PaintCommandV2, pushApplied(), guard band system

**Engine** (`src/engine/`):
- `raster/` — Pixel-level operations: stamp blitting, alpha compositing, blend modes
- `brush/` — Brush presets, dynamics (pressure/size/opacity), paper texture, stabilizer
- `vector/` — BezierPath: cubic curves, evaluateAt(), flattenPoints(), curveTo()
- `animation/` — Timeline playback, onion skin rendering, tween engine

**UI** (`src/ui_v2/`):
- `canvas_widget_v2.cpp` — The beast: paintEvent (layers + onion + grid + previews), all 11 tools, undo/redo, keyboard shortcuts, text editing, pick color feedback
- `main_window_v2.cpp` — Orchestrator: dock widgets, top toolbar, signal connections, file ops
- `property_editor_v2.cpp` — Context-sensitive property panel: brush, pick color, fill, text
- `layer_panel_v2.cpp` — QListWidget-based layer stack with inline rename
- `toolbox_panel_v2.cpp` — Vertical tool palette with color swatch
- `timeline_panel_v2.cpp` — Frame thumbnails, playback controls

---

## Problem-Solution Map (Cumulative)

| # | Problem | Solution | Commit |
|---|---------|----------|--------|
| 1 | Ghost outlines after Ctrl+Z | cleanLayerCopy_ for before-snapshot | `0f92cf1` |
| 2 | Dirty rect artifact on commit | pushApplied() instead of pushCommand()->redo() | `8ab7b21` |
| 3 | Buffer ballooning on move | ensureContains(pad=false) for exact expansion | `0aa9e07` |
| 4 | Content clipping beyond canvas | Auto-expand layer on move | `c72a85a` |
| 5 | Qt raster tile seams | Anti-alias disable + pixel-snapping + padded cache | `cced79d` |
| 6 | Undo capture rect mismatch | Exact requested rect with zero-fill for OOB | `06ceb10` |
| 7 | Guard band & alpha compositing | kGuardBand=2, offscreen stamp, premultiplied blend | `d98eb53` |
| 8 | Keyboard shortcuts broken | Full shortcut map in keyPressEvent | `3cc22e2` |
| 9 | Only brush tool implemented | All 11 tools in V2 canvas | `d010f63` |
| 10 | Brush controls scattered | PropertyEditorV2 unification | `9391248` |
| 11 | Pick color no visual feedback | CrossCursor + color preview circle | *this session* |
| 12 | No layer rename in V2 | Double-click → QLineEdit inline | *this session* |
| 13 | Color variations stale on pick | updateColorVariations() on colorPicked | *this session* |
| 14 | Text tool required modal dialogs | In-canvas editing + Property Editor controls | *this session* |

---

## Testing Status

- **Test framework**: GoogleTest
- **Test count**: 139 tests
- **Pass rate**: 100% (0 failures)
- **Test categories**: Document, Timeline, Layer, Brush Engine, Bezier, File Format, Raster/Vector Layer, Layer Memory, Compositor, Undo, Deformation, Node Graph, Audio, Ruler, Tween, Pencil Retouch, ABR Importer
- **Build target**: `free-animation-2d-style` (Release, MSVC 2022)

---

## Build Commands

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

---

## Next Steps (Suggested)

1. **Text tool enhancements**: font preview dropdown showing rendered samples, text alignment (left/center/right), text bounding box drag to reposition before commit
2. **Onion skin / Canvas size moved to top bar**: The existing design keeps them in the toolbox — consider moving to a dedicated top toolbar section
3. **Selection tool improvements**: transform handles (scale, rotate), feathering
4. **Vector layer rendering**: VectorLayer currently stored but not rendered in paintEvent
5. **Audio waveform display**: Audio tracks exist in data model but lack visual waveform in timeline
6. **Multi-language support**: All strings currently hardcoded in English
