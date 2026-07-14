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
  - `file_association.{hpp,cpp}` — Windows registry .fap file association
- `scripts/` — Build helpers
  - `embed_icon.py` — Post-build Win32 icon resource injector
- `tests/` — GoogleTest test files
- `docs/` — Architecture and build documentation
- `docs/archive/` — Archived handoffs, old session reports
- `resources/` — Brush tips, palettes, paper textures

## Code Conventions

- Namespace: `fap`
- C++20, CMake 3.20+, Qt 6.5+
- Headers use `#pragma once`
- Engine code is Qt-independent (no QObject/Qt types in engine/)
- UI code uses Qt 6 Widgets with dark theme (#252830 panels, #000000 toolbar/titlebar, #FF4800 accent)
- Font: Avenir LT Std (bundled .otf + .ttc), loaded via QRC at startup
- Window icon: logo.png (256x256) set via setWindowIcon(), appears in title bar / taskbar
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

Work area bar: 4px fill `QColor(255, 72, 0, 80)` with 1px border lines `QColor(255, 72, 0, 180)` at in/out boundaries.

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

## Bug Fix Session (v2.5.2 — Jul 2026)

### #10 — Close confirmation for unsaved changes
**Symptom**: Closing the window (X, Alt+F4) discarded unsaved work silently.

**Fix**: Added `closeEvent(QCloseEvent*) override` in `MainWindowV2`. If `isModified()`, shows Save/Discard/Cancel dialog. Save calls `saveProject()`, Discard closes, Cancel stays.

**Files**: `src/ui_v2/main_window_v2.hpp:42`, `src/ui_v2/main_window_v2.cpp:525-548`

### #11 — FFmpeg `waitForFinished(-1)` infinite hang
**Symptom**: If FFmpeg hung during video export, the entire app froze indefinitely.

**Fix**: Changed `proc.waitForFinished(-1)` to `proc.waitForFinished(120000)` with `proc.kill()` on timeout in `executeFFmpeg()`.

**Files**: `src/io/video_export.cpp:87`

### #2 — NodeGraph::evaluate ignores target parameter
**Symptom**: `evaluate(RasterLayer& target)` computed the graph but never wrote the result to `target`. The output was discarded.

**Fix**: After `evaluateNode(output, cache)`, copies the OutputNode's cached `RasterLayer` pixel data into `target` via `std::memcpy` row-by-row, respecting `std::min` of dimensions. Sets `bufferEpochTick()` and `setHasContent(true)`.

**Files**: `src/engine/compositor/node_graph.cpp:376-397` (+11 lines)
**Tests**: `NodeGraphTest.EvaluateWritesToTarget`, `EvaluateWithDisabledInputSkipsTarget`, `EvaluateNoOutputNodeReturnsUnchanged`

### #8 — Legacy load omits `setHasContent(true)`
**Symptom**: Loading v1/v2/v3 legacy .fap files: pixel data loaded correctly but `hasContent_` was never set. Timeline cells appeared empty.

**Fix**: Added `rl->setHasContent(true)` after successful PNG pixel copy in all 3 legacy load paths (v1, v2, v3).

**Files**: `src/io/file_format.cpp:243,283,328` (3 lines)

### Multi-sequence data loss on load (CRITICAL)
**Symptom**: Projects with 2+ sequences lost all layer content on reopen. Layer names reverted to defaults.

**Root cause**: `readTimeline()` called `doc.addSequence("")` for si>0, creating DUPLICATE sequences. But `readManifest()` already created them correctly. The cleanup loop removed the duplicates (which had the data), leaving the originals empty.

**Fix**: `readTimeline()` now uses `doc.sequenceAt(si)` instead of `doc.addSequence("")`. Guard: if `si >= doc.sequenceCount()`, calls `addSequence("")` as fallback.

**Files**: `src/io/document_manager.cpp:604-607` (4 lines)
**Tests**: `FileFormatTest.BinaryZipSavesLayerNames`

### Layer rename lost on focus change (UI)
**Symptom**: Renaming a layer and clicking another layer without pressing Enter: the name was silently discarded.

**Root cause**: The `editingFinished` signal was disconnected (fix for a previous double-delete QLineEdit crash). When the editor lost focus, no commit happened.

**Fix**: Reconnected `editingFinished` with a `std::make_shared<bool> committed` guard. `returnPressed` sets `committed = true` first; `editingFinished` checks the guard. Both signals can fire safely — only the first one commits.

**Files**: `src/ui_v2/layer_panel_v2.cpp:462-476`

### Video export enhancements (H.264 MP4 + MOV/WebM with alpha)
- Created `src/io/video_export.hpp` — public API for `renderExportFrame()`, `exportVideo()`, `exportGIF()`, `exportPNGSequence()`
- `renderExportFrame()` extracted from anonymous namespace, added `transparentBg` parameter
- `MainWindowV2::exportVideo()` now uses `renderExportFrame()` for clean compositing (no UI overlays)
- File dialog now offers: MP4 H.264, MOV QuickTime with Alpha (qtrle+argb), WebM VP9 with Alpha (libvpx-vp9+yuva420p)
- Format auto-detected from file extension, codec+background selected accordingly

**Files**: `src/io/video_export.hpp` (new), `src/io/video_export.cpp`, `src/ui_v2/main_window_v2.cpp:793-871`

**Tests**: 159/159 pass.

## Bug Fix Session (v2.6 — Jul 2026)

### #12 — Eraser tool produces no effect on any layer
**Symptom**: After drawing on multiple layers, selecting the eraser tool and attempting to erase had no visible effect.

**Root cause**: In `drawBrushStamp()`, the erasing branch applied a `DestinationOut`-like blend (`dst * (255 - srcA) / 255`) onto `strokeBuffer_`, which is initialized with transparent pixels (`fill(0)`). Since 0 × anything = 0, stamps never accumulated. At `commitStroke()`, the empty `strokeBuffer_` was composited with `CompositionMode_DestinationOut` — erasing nothing.

**Fix**: Removed the broken erasing branch. Both brush and eraser now use the same `SourceOver` blend to accumulate stamps in `strokeBuffer_`. The distinction happens in `renderStampToImage()` (white stamps vs color stamps) and `commitStroke()` (`DestinationOut` vs `SourceOver`).

**Files**: `src/ui_v2/canvas_widget_v2.cpp:2157-2176` (removed 11 lines, unified blend)

### #13 — Layer names overlap visually after frame change
**Symptom**: When switching frames or clicking a different layer during an in-progress rename, old layer name labels remained visible and overlapped with new ones, showing "LINEA" and "Layer" simultaneously.

**Root cause**: `QListWidget::clear()` deletes `QListWidgetItem` objects but does NOT destroy `QWidget` children set via `setItemWidget()`. Old `QLabel` widgets remained as invisible orphans, overlapping new widgets on the next paint.

**Fix**: Before `list_->clear()`, iterate all items and call `removeItemWidget()` + `hide()` + `deleteLater()` on each widget. Using `deleteLater()` (not `delete`) prevents crashes when in-progress rename lambdas still hold raw pointers to these widgets.

**Files**: `src/ui_v2/layer_panel_v2.cpp:502-514`

### #14 — App crash during layer rename
**Symptom**: The application crashed abruptly after renaming a layer and then clicking another layer or frame.

**Root cause**: The `startRename()` lambda captured raw pointers (`QHBoxLayout*`, `QLineEdit*`, `QLabel*`) to widgets that could be deleted by `refreshLayerList()` before the lambda's `editingFinished` signal fired. Accessing deleted widgets → use-after-free crash.

**Fix**: Changed raw pointer captures to `QPointer<QHBoxLayout>`, `QPointer<QLineEdit>`, `QPointer<QLabel>`. The lambda now checks `if (!pHlay || !pEditor || !pNameLabel) return;` before accessing any widget.

**Files**: `src/ui_v2/layer_panel_v2.cpp:462-466`

### #15 — Video export: unified format support + audio mixing
**Symptom**: Video export code was duplicated across `video_export.cpp` and `MainWindowV2::exportVideo()`. Format detection, codec selection, and frame rendering existed in two places. MOV/WebM alpha formats only worked from MainWindowV2, not from the public API. No audio was included in exports.

**Fix:**
- `exportVideo()` signature simplified to `exportVideo(doc, outputPath, fps)` — format auto-detected from file extension
- All 3 formats unified: MP4 H.264, MOV QuickTime Alpha (qtrle+argb), WebM VP9 Alpha (libvpx-vp9+yuva420p)
- Audio mixing: non-muted tracks with individual volumes → `amix` filter, codec per container (aac/pcm_s16le/libopus), `-shortest` for duration
- `MainWindowV2::exportVideo()` reduced from 80 lines to 20 — delegates to `fap::exportVideo()`
- GIF export also unified: `MainWindowV2` now calls `fap::exportGIF()` instead of 60 lines of duplicated inline code
- `exportGIF()` fixed: removed hardcoded 320px scale, exports at full canvas resolution, corrected 1-based frame indexing

**Files**: `src/io/video_export.hpp`, `src/io/video_export.cpp`, `src/ui_v2/main_window_v2.cpp:793-818`

### #16 — Playback timer imprecision (FPS drift)
**Symptom**: The program's animation playback appeared slightly slower than the exported GIF/video. The FPS display was correct but actual timing was off.

**Root cause**: Timer interval used integer division `1000 / fps_`. At 24fps this produced 41ms instead of 41.67ms, compounded by `buildBackgroundCache()` rendering overhead for multi-layer 1080p compositions.

**Fix**: Changed timer interval to `static_cast<int>(std::round(1000.0 / fps_))` in both `setFPS()` and the play button handler. Now uses floating-point math with rounding for better accuracy.

**Files**: `src/ui_v2/timeline_panel_v2.cpp:1134,1177`

### #17 — Copy layer to all frames (new feature)
**Feature**: Button in the layer panel footer copies the selected layer's content, name, and properties to the same layer index across every frame in the sequence.

**Implementation:**
- `Sequence::copyLayerToAllFrames(sourceFrame, layerIndex)` — copies metadata + pixel data to all frames via `shareDataFrom()` + `ensureUnique()` (independent copies)
- `CopyLayerToFramesCommand` — full undo/redo support with per-frame backups
- UI button with tooltip in `LayerPanelV2` footer row (reuses duplicate icon)

**Files**: `src/core/sequence.hpp:71`, `src/core/sequence.cpp:218-255`, `src/ui_v2/layer_panel_v2.hpp:47`, `src/ui_v2/layer_panel_v2.cpp`

### #18 — Windows .fap file icon + file association
**Feature**: Custom icon for `.fap` files in Windows Explorer.

**Implementation:**
- `resources/icons/fap.ico` — 7 resolutions (16-256px) generated from project logo PNGs
- `scripts/embed_icon.py` — post-build Python script using Win32 `BeginUpdateResource`/`UpdateResource`/`EndUpdateResource` to inject the .ico into the .exe
- `src/platform/file_association.{hpp,cpp}` — registry-based `.fap` → `FAP.Document` association (`HKCU\Software\Classes`)
- `registerFileAssociation()` called from `MainWindowV2` constructor on startup

**Files**: `resources/icons/fap.ico` (new), `scripts/embed_icon.py` (new), `src/platform/file_association.hpp` (new), `src/platform/file_association.cpp` (new), `src/ui_v2/main_window_v2.cpp:102,110-115`, `CMakeLists.txt`

### Timeline Architecture — v2.6 addendum

**Cross-frame layer operations** (`src/core/sequence.cpp:218`):
- `copyLayerToAllFrames(int sourceFrame, int layerIndex)` — copies layer name, visibility, opacity, blend mode, lock, pixel data, and `hasContent_` flag from a source frame to all other frames
- Uses `shareDataFrom()` + `ensureUnique()` pattern: target layers share the source buffer temporarily, then deep-copy via COW
- Target frames with fewer layers are auto-expanded to match

**Video export pipeline** (`src/io/video_export.cpp`):
- Unified `exportVideo()` auto-detects container from extension
- Audio mixing via FFmpeg `amix` filter with per-track volume
- `executeFFmpeg()` with 120s timeout (was infinite) and `.kill()` on timeout
- `exportGIF()` uses full canvas resolution (was hardcoded 320px)

**Layer panel robustness** (`src/ui_v2/layer_panel_v2.cpp`):
- `refreshLayerList()` now properly cleans up old `setItemWidget()` widgets before clearing
- `startRename()` uses `QPointer` guards to prevent use-after-free crashes

**Frame clipboard — right-click context menu** (`src/ui_v2/timeline_panel_v2.cpp`):
- Right-click on any frame cell in the timeline → Copy/Cut/Paste menu
- Clipboard stores complete `GroupLayer` clone (all layers + pixel data)
- `CutFrameCommand` and `PasteFrameCommand` provide full undo/redo
- Explicit `~TimelinePanelV2()` destructor needed for `unique_ptr<GroupLayer>` member

**Tests**: 160/160 pass.
