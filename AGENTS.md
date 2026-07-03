# AGENTS.md - AI Agent Instructions

## Project: Free Animation Power

2D animation software with hybrid vector + raster engine. Built with C++20 and Qt 6.

## Build Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

## Project Structure

- `src/core/` — Data model (Document, Layer, Timeline, Types)
- `src/engine/raster/` — Bitmap pixel engine
- `src/engine/vector/` — Bezier path vector engine
- `src/engine/brush/` — Brush engine with paper texture
- `src/engine/animation/` — Timeline playback, onion skinning
- `src/ui/` — Qt widgets (Canvas, panels, MainWindow)
- `src/io/` — File format (.fap), video export
- `src/platform/` — Input handling, tablet support
- `src_py/` — Python prototype (reference implementation)
- `tests/` — GoogleTest test files
- `docs/` — Architecture and build documentation
- `resources/` — Brush tips, palettes, paper textures

## Code Conventions

- Namespace: `fap`
- C++20, CMake 3.20+, Qt 6.5+
- Headers use `#pragma once`
- Engine code is Qt-independent (no QObject/Qt types in engine/)
- UI code uses Qt 6 Widgets with dark theme (#2d2d30 background, #d4782a accent)
- No external dependencies beyond Qt, FFmpeg, and GoogleTest

## Skills Available

37 agent skills in `.agents/skills/`. Key ones:
- `tdd` — Test-driven development workflow
- `improve-codebase-architecture` — Architecture analysis
- `design-an-interface` — API design
- `ui-ux-pro-max` — UI/UX design intelligence
- `diagnose` — Bug diagnosis
- `review` — Code review
- `setup-pre-commit` — Pre-commit hooks

## Display Pipeline Architecture (Jul 2026)

**Solution implemented** — 4-buffer CPU rendering pipeline with strict DATA/DISPLAY separation:

| Buffer | Domain | Contents |
|--------|--------|----------|
| `RasterLayer::pixelBuffer_` | DATA | Per-layer strokes, transparent bg, source of truth |
| `strokeBuffer_` | DATA | Isolated current stroke, transparent ARGB32 |
| `backgroundCache_` | DISPLAY | Pre-composited: white bg + onion skin + all layers |
| `paintEvent` | DISPLAY | Composes cache + stroke overlay + grid + UI overlays |

Key invariants:
- `mouseReleaseEvent` NEVER reads from display buffers (only `strokeBuffer_` and `pixelBuffer_`)
- Dabs written to isolated `strokeBuffer_` during stroke, composited onto `pixelBuffer_` on commit
- `buildBackgroundCache(rect)` supports both full and partial (offset-aware dirty rect) rebuilds
- Dynamic padding: `brushSize/2 + 1` to cover dab feathering edges
- Layers with `originX_`, `originY_` offsets use `rect.translated(-originX, -originY)` — no forced FULL rebuild

**Session report**: `docs/session-report-2026-07-02.md` — full post-mortem of the 2-day debugging session, rollback, and final architecture solution.

## Timeline Panel Architecture (v2.3 — Jul 2026)

**Non-destructive rebuild + free track movement** in `src/ui_v2/timeline_panel_v2.cpp`:

| Component | Domain | Rebuild behavior |
|-----------|--------|-----------------|
| `SequenceTrackWidget` | Core (`Document::sequences()`) | Destroyed & recreated from `AppState` |
| `AudioTrackWidget` | UI-only (`audioTrackWidgets_`) | Extracted, preserved, reinserted (no delete) |

### Key invariants

- `rebuildTracks()` NEVER calls `delete` on `AudioTrackWidget*` — uses `removeWidget` + `addWidget` cycle
- `onMoveAudioTrack()` uses `takeAt`/`insertItem` for free layout movement (no zone restriction, no separator)
- `QTimer::singleShot(0, ...)` defers all sequence mutations before `rebuildTracks()`
- `QApplication::focusWidget()->clearFocus()` protects `QLineEdit` from focus-loss crashes
- `tracksLayout_->update()` called before `widget->update()` — forces synchronous geometry before waveform repaint
- `ScrollBarAlwaysOn` for vertical scrolling, horizontal handled by `sharedScrollOffset_` + custom `hScrollBar_`

### Layout structure
```
Top bar (fixed) → Ruler (fixed)
  → QScrollArea [tracksContainer_ with QVBoxLayout]
    → SequenceTrackWidget[]  (from Document, index order)
    → AudioTrackWidget[]     (preserved, freely movable)
    → QSpacerItem (stretch)
  → QScrollBar horizontal (fixed)
```

**Session report**: `docs/session-report-2026-07-03.md` — non-destructive rebuild, free audio movement, waveform fix.

## Testing

Use `tdd` skill for new features. All tests in `tests/` directory.
Test files use GoogleTest framework. Run with:
```bash
cmake --build build --target fap-tests
./build/fap-tests
```
