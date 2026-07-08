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
- `src/ui_v2/` — Qt widgets (Canvas, panels, MainWindow)
- `src/io/` — File format (.fap binary ZIP), video/SVG export
  - `document_manager.cpp` — Atomic save/load via miniz (ZIP renamed to .fap)
  - `file_format.cpp` — Legacy directory-based .fap (v1/v2/v3 backward compat)
  - `video_export.cpp` — FFmpeg MP4/GIF export
  - `svg_export.cpp` — SVG export (single frame + multi-frame)
- `src/platform/` — Input handling, tablet support
- `tests/` — GoogleTest test files
- `docs/` — Architecture and build documentation
- `docs/archive/` — Archived handoffs, old session reports
- `resources/` — Brush tips, palettes, paper textures

## Code Conventions

- Namespace: `fap`
- C++20, CMake 3.20+, Qt 6.5+
- Headers use `#pragma once`
- Engine code is Qt-independent (no QObject/Qt types in engine/)
- UI code uses Qt 6 Widgets with dark theme (#252830 background, #FF6B4A accent)
- No external dependencies beyond Qt, FFmpeg, GoogleTest, and miniz (zlib compression)

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

**Session report**: `docs/archive/session-report-2026-07-02.md` — full post-mortem of the 2-day debugging session, rollback, and final architecture solution.

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

**Session report**: `docs/archive/session-report-2026-07-03.md` — non-destructive rebuild, free audio movement, waveform fix.

### Waveform Decoding Architecture (Jul 2026)

**dr_libs** native decoders (see Audio Decoder Architecture below). `decodeAudio()` calls `decodeAudioFile()` which returns `AudioDecodeResult { peaks, sampleRate, channels, durationMs }`. Synchronous, ~10ms for 3-min song.

**Guards**:
- `if (w <= hdrW) return;` in drawWaveform — prevents division-by-zero during takeAt/insertItem layout moves
- `decodeError_` flag sets paintEvent message to "Unsupported format" vs "No waveform data"

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
- `setFps(fps)` → `activeSequence().setFPS(fps)` (guard: no-op if same value)

### Display pipeline updates

| Component | Change |
|-----------|--------|
| `RulerWidget::paintEvent()` | WA shading bar between in/out points |
| `SequenceTrackWidget::paintEvent()` | Cells render up to `durationFrames_` (empty beyond `totalFrames_`) |
| `updateScrollBarRange()` | Horizontal scroll uses `durationFrames_` instead of `totalFrames_ + 1` |
| `onPlaybackTick()` | Calls `Sequence::advanceFrame()` for WA-aware looping |
| `PropertyEditorV2` | SEQUENCE + DURATION group **removed** (Jul 2026 UI cleanup) — now Timeline-only |

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

### FPS — unidirectional pipeline

| Trigger | Action |
|---------|--------|
| `fpsMinusBtn_` (−) / `fpsPlusBtn_` (+) | Lambda → `appState_->setFps(fps ± 1)` clamped [1, 120]. Styled: dark theme, 20x22 fixed. |
| `fpsSpin_` → `onFPSChanged` | → `appState_->setFps(value)`. `NoButtons` — no native arrow overlays. |
| `AppState::setFps()` → `emit documentChanged()` | → lambda syncs `fpsSpin_` + timer interval + `setPlaybackRate(fps/24.0)` |
| `documentChanged` listener | `if (fps_ != seqFps)` guard — prevents re-entrant spinbox update |

### Keyboard shortcuts

| Key | Action |
|-----|--------|
| `Shift+I` | Set Work Area In point to current frame |
| `Shift+O` | Set Work Area Out point to current frame + 1 |

**Session report**: `docs/archive/session-report-2026-07-04.md` — v2.4 full feature spec, WA interactivity, audio transport sync.

### Consolidated architecture report
`docs/report-acetato-nle-2026-07-03.md` — comprehensive report covering all v2.2-v2.4 features (multi-sequence, audio, waveform, non-destructive rebuild, WA, FPS, duration).

### Complete project report (Spanish)
`docs/INFORME_COMPLETO_FAP.md` — exhaustive architecture documentation, data model, engine layer, display pipeline, UI V2, IO, tests, bug history.

### Frame Content Detection — O(1) `hasContent_` flag (Jul 2026)

**RasterLayer** now carries a `hasContent_` boolean that tracks whether the layer contains non-transparent pixels:

| Operation | `hasContent_` behavior |
|-----------|----------------------|
| `setPixel()` with alpha > 0 | Set to `true` |
| `blendPixel()` with result alpha > 0 | Set to `true` |
| `clear()` | Reset to `false` |
| `clone()` | Preserved |
| `commitStroke()` / `doText()` / `commitMove()` / `commitFloatingSelection()` | Set to `true` at commit time |

**Key invariant**: `hasContent_` is NOT set in per-pixel hot paths (`setPixel`/`blendPixel`) because the brush engine writes directly to `pixelSpan()` via QPainter, bypassing those methods. Instead, the flag is set at stroke **commit time** in `CanvasWidgetV2::commitStroke()` and sibling methods.

`SequenceTrackWidget::frameHasContent()` reads `rl->hasContent()` in O(1) — no pixel buffer scan.

### Waveform layout invariant

`rebuildTracks()` now calls `tracksLayout_->update()` synchronously after `addStretch()` — matching the same guard already in `refreshTimelineLayout()`. This ensures `AudioTrackWidget::paintEvent()` receives correct `width()` for waveform rendering.

## Testing

Use `tdd` skill for new features. All tests in `tests/` directory.
Test files use GoogleTest framework. Run with:
```bash
cmake --build build --target fap-tests
./build/fap-tests
```

## Language Policy

| Scope | Language |
|-------|----------|
| Source code (comments, identifiers) | English |
| `docs/architecture.md` | English |
| `AGENTS.md` | English |
| `README.md` | Spanish |
| `docs/INFORME_COMPLETO_FAP.md` | Spanish |
| `docs/report-acetato-nle-2026-07-03.md` | Spanish |
| `docs/build-instructions.md` | English |
| `CHANGELOG.md` | Spanish |

## Audio Decoder Architecture (Jul 2026)

**dr_libs** (David Reid) — Public domain native decoders replacing broken QAudioDecoder:

| Library | Format | API |
|---------|--------|-----|
| `dr_wav.h` | WAV (PCM, ADPCM, Float) | `drwav_open_file_and_read_pcm_frames_f32` |
| `dr_mp3.h` | MP3 (MPEG1/2 Layer 1-3) | `drmp3_open_file_and_read_pcm_frames_f32` |
| `dr_flac.h` | FLAC + Ogg/FLAC | `drflac_open_file_and_read_pcm_frames_f32` |

Wrapper: `src/engine/animation/audio_file_decoder.cpp` — `decodeAudioFile(path, targetPeaks=800)`.
Synchronous, ~10ms for 3-min song. Returns `AudioDecodeResult { peaks, sampleRate, channels, durationMs }`.

## File Format Architecture (.fap v2 — Jul 2026)

**Binary ZIP format** via `DocumentManager` in `src/io/document_manager.cpp`:

```
.fap file (renamed .zip)
├── manifest.json       — version, canvas size, active_sequence, viewport (zoom/offset)
├── timeline.json       — per-sequence: frames[], layers[] with pixel_file/meta_file paths
└── layers/S{seq}/frame_{f}/layer_{ll}.png + .json
```

Key features:
- Atomic save: write to .tmp, rename on success
- Pixel deduplication: shared `pixelBuffer_` pointers detected via `unordered_set`
- ViewState (zoom, offsetX, offsetY) persisted in manifest
- 11 metadata fields per sequence: name, fps, totalFrames, visible, opacity, locked, workAreaStart/End, durationFrames, looping, currentFrame
- Layer metadata: origin_x/y, hasContent, opacity, blendMode, locked
- Backward compatible: legacy directory format v1/v2/v3 still loadable via `file_format.cpp`

## Pixel Offset Bug — Load Path (v2.5.1 — Jul 2026)

**Symptom**: Content drawn at canvas center appears at top-left corner (0,0) after save→reopen.

**Root cause**: `readLayerData()` in `document_manager.cpp` called `ensureContains()` with default `pad=true`. This added `kGuardBand` (2px) + `kGrowthPad` (512px), expanding the buffer and shifting the origin on every load. The subsequent PNG pixel copy wrote at row 0 of the expanded buffer, placing all pixels at the wrong canvas offset.

**Fix**: Pass `pad=false` in the load path (`document_manager.cpp:617`). The save path already writes correct `origin_x`/`origin_y` via `layerMetadataToJson()`. The load path already restores them via `layerMetadataFromJson()` → `setOrigin(x,y)`. The only missing piece was preventing unnecessary buffer re-expansion during load.

**Invariant**: `ensureContains(..., false)` is a no-op when the PNG fits within existing buffer dimensions. Guard band is only needed during active drawing (dab feathering).

**Files affected**: `src/io/document_manager.cpp` (1 line, 1 character change).
**Session report**: `docs/archive/session-report-2026-07-08.md`
**Tests**: 154/154 pass.

## Tool State Desync on Load (v2.5.1 — Jul 2026)

**Symptom**: After opening a .fap file, the brush tool behaves as an eraser on new layers. The toolbox UI shows Brush as selected, but drawing erases instead of painting.

**Root cause**: `MainWindowV2::openProject()` calls `resetDocument()` which calls `tool_state_->resetToDefaults()` → `setActiveTool(ToolType::Eraser)`. The `activeToolChanged` signal is emitted but nothing in MainWindowV2 is connected to sync the toolbox `QButtonGroup`. The canvas reads `appState_->toolState().activeTool()` directly on each mouse event, so it picks up Eraser while the UI shows Brush.

**Fix**: Call `toolbox_panel_->setActiveTool(0)` at the end of `openProject()` success branch. This syncs both the UI button group and the ToolState back to Brush.

**Files affected**: `src/ui_v2/main_window_v2.cpp` (1 line).
**Invariant**: Canvas queries `appState_->toolState().activeTool()` directly, not from a cached variable. Toolbox UI must be explicitly synced after any programmatic ToolState changes.

## Audio Persistence Architecture (v2.5.1 — Jul 2026)

**Audio embedded in .fap ZIP** — import, save, reopen round-trip with full metadata.

### Data model (`src/core/document.hpp`)

```cpp
struct AudioTrackData {
    std::string filepath;      // original path or extracted temp path
    std::string displayName;   // e.g., "song.mp3"
    std::string zipEntry;      // "audio/track_0.mp3" inside ZIP
    bool muted = false;
    int volume = 80;           // 0-100
};
// Document owns: std::vector<AudioTrackData> audioTracks_
```

### Save path (`src/io/document_manager.cpp`)

| Step | Method | What it does |
|------|--------|-------------|
| Metadata | `writeTimeline()` | Serializes `"audio"` array in `timeline.json` (filepath, display_name, zip_entry, muted, volume) |
| Bytes | `writeLayerData()` | Reads audio file from disk, embeds as `audio/track_N.ext` ZIP entry |

### Load path (`src/io/document_manager.cpp`)

| Step | Method | What it does |
|------|--------|-------------|
| Metadata | `readTimeline()` | Parses `"audio"` array → `doc.addAudioTrack()` |
| Extract | `extractAudio()` | Decompresses ZIP entries to `%TEMP%/fap_audio_<PID>/`, updates `filepath` to temp path |

### UI (`src/ui_v2/`)

| Component | Method | Role |
|-----------|--------|------|
| `AudioTrackWidget` | `isMuted()`, `setMuted()`, `volume()`, `setVolume()` | State getters/setters for save/restore |
| `TimelinePanelV2` | `addAudioTrackFromData()` | Creates widget from `AudioTrackData` |
| `TimelinePanelV2` | `clearAudioTracks()` | Removes all audio widgets (new project cleanup) |
| `TimelinePanelV2` | `onImportAudio()` | Creates widget + registers with `Document::addAudioTrack()` |
| `MainWindowV2` | `syncAudioToDocument()` | Collects widget state into Document before save |
| `MainWindowV2` | `openProject()` | After load: iterates `doc.audioTracks()`, calls `addAudioTrackFromData()` |

### Key invariants
- Audio files are **fully embedded** in .fap ZIP — portable across machines
- `extractAudio()` runs AFTER `readLayerData()` and BEFORE `mz_zip_reader_end()`
- `syncAudioToDocument()` MUST be called before every `dm.save()` to capture live state
- `clearAudioTracks()` MUST be called in `newProject()` before `rebuildTracks()` to prevent audio leaking between projects
- `onImportAudio()` registers the track with Document — state survives save/reopen without extra sync

### ZIP structure
```
.fap (ZIP renamed)
├── manifest.json
├── timeline.json       ← + "audio" array
├── audio/track_0.mp3   ← embedded audio bytes
├── audio/track_1.wav
└── layers/S0/...
```

## Brush Tool Initialization (v2.5.1 — Jul 2026)

**Invariant**: Both `openProject()` and `newProject()` MUST call `toolbox_panel_->setActiveTool(0)` after `resetDocument()`. `ToolState::resetToDefaults()` sets active tool to `Eraser`, and the `QButtonGroup` in the toolbox is not automatically synced from `activeToolChanged` signal. The canvas reads `toolState().activeTool()` directly on each mouse event, so it picks up Eraser while the UI shows Brush.

**Files affected**: `src/ui_v2/main_window_v2.cpp` — lines 582 (openProject) and 522 (newProject).
