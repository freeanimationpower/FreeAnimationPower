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

### Waveform Decoding Architecture (Jul 2026)

**QAudioDecoder** in `AudioTrackWidget::decodeAudio()` — adaptive 5-branch handler:

| Buffer format | Backend | Handling |
|--------------|---------|----------|
| Float | WMF (MP3), FFmpeg | `std::abs(samples[i])` |
| Int32 | WMF (24-bit WAV) | `abs / 2147483648.0f` |
| Int16 | DirectSound, some WAV codecs | `abs / 32768.0f` |
| UInt8 | Legacy codecs | `(sample - 128.0f) / 128.0f` |
| Unknown (else) | Any | `peak = 0.2f` (visible fallback) |

**Key invariant**: `decoder->setAudioFormat()` is NEVER called. The decoder uses its backend's native format. Forcing Int16 on WMF silently blocks bufferReady emission for MP3.

**Guards**:
- `if (w <= hdrW) return;` in drawWaveform — prevents division-by-zero during takeAt/insertItem layout moves
- `if (!buffer.isValid() || buffer.sampleCount() == 0) return;` in bufferReady
- `decodeError_` flag sets paintEvent message to "Decoder error" vs "No waveform data"

## Timeline Panel Architecture (v2.4 — Jul 2026)

**Real FPS + Work Area + Duration Control + Transport Sync** in `src/ui_v2/timeline_panel_v2.cpp`:

### Sequence model extensions (`src/core/sequence.hpp`)

| Member | Default | Purpose |
|--------|---------|---------|
| `workAreaStart_` | 0 | In point for loop/playback (frame index) |
| `workAreaEnd_` | 0 | Out point (0 = use `totalFrames_` as end) |
| `durationFrames_` | = totalFrames_ | Absolute timeline scrollable width |

Key methods:
- `effectiveWorkAreaEnd()` — `workAreaEnd_ > 0 ? min(workAreaEnd_, totalFrames_) : totalFrames_`
- `advanceFrame()` — advances 1 frame within work area bounds, wraps to `workAreaStart_` at end. Qt-free, engine-level.
- `clone()` preserves all 3 new fields.

### RulerWidget interactivity (`timeline_panel_v2.cpp`)

| Interaction | Behavior |
|------------|----------|
| Hover ±5px of WA edge | Cursor changes to `SizeHorCursor` |
| Click ±5px of In/Out edge | Start drag for respective WA boundary |
| Click elsewhere | Playhead click (sets frame) + drag = scrubbing |
| Drag WA In edge | Resize in real-time, clamp `[0, waEnd-1]` |
| Drag WA Out edge | Resize in real-time, clamp `[waStart+1, totalFrames]` |
| Click during playback | Auto-pauses playback + audio before moving playhead |

Work area bar: 4px fill `QColor(255, 107, 74, 80)` with 1px border lines `QColor(255, 107, 74, 180)` at in/out boundaries.

### AppState bridges (centralized state pipeline)

All mutations go through `AppState` → `emit documentChanged()`:
- `setWorkAreaStart(frame)` → `activeSequence().setWorkAreaStart()` 
- `setWorkAreaEnd(frame)` → `activeSequence().setWorkAreaEnd()`
- `setDurationFrames(count)` → `activeSequence().setDurationFrames()`

### Display pipeline updates

| Component | Change |
|-----------|--------|
| `RulerWidget::paintEvent()` | WA shading bar between in/out points |
| `SequenceTrackWidget::paintEvent()` | Cells render up to `durationFrames_` (empty beyond `totalFrames_`) |
| `updateScrollBarRange()` | Horizontal scroll uses `durationFrames_` instead of `totalFrames_ + 1` |
| `onPlaybackTick()` | Calls `Sequence::advanceFrame()` for WA-aware looping |
| `PropertyEditorV2` | SEQUENCE group with DURATION spinbox (1-10000 fr), `refreshSequenceFields()` |

### Audio transport sync

| Trigger | Action |
|---------|--------|
| Play | `setPlaybackRate(fps/24.0)` + `syncToFrame()` + `play()` |
| Pause (||) | `playbackTimer_->stop()` + `player()->pause()` on all audio tracks |
| Stop (■) | `player()->stop()` + position reset to 0 |
| FPS change | `setPlaybackRate(fps/24.0)` on all tracks, timer interval `1000/fps` |
| WA loop (looping=true) | `syncToFrame(waStart)` — re-syncs audio position |
| WA end (looping=false) | Auto-stop: timer stop + `player()->pause()` + stay at last WA frame |
| Sequence switch | `setPlaybackRate(newFps/24.0)` on all tracks |
| Ruler click during playback | `togglePlayback()` → pauses before moving playhead |

Guard: `updatingFps_` flag prevents signal feedback loop on `fpsSpin_` ↔ `onFPSChanged`.

### Keyboard shortcuts

| Key | Action |
|-----|--------|
| `Shift+I` | Set Work Area In point to current frame |
| `Shift+O` | Set Work Area Out point to current frame + 1 |

**Session report**: `docs/session-report-2026-07-04.md` — v2.4 full feature spec, WA interactivity, audio transport sync.

## Testing

Use `tdd` skill for new features. All tests in `tests/` directory.
Test files use GoogleTest framework. Run with:
```bash
cmake --build build --target fap-tests
./build/fap-tests
```
