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

## Bug Fix Session (v2.6.1 — Jul 2026)

### Critical Fixes

#### C1 — ABR importer endian swap broken (silent failure on x86)
**Symptom**: All ABR v1/v2 brush files failed to load silently on little-endian machines.

**Root cause**: `uint16_t` promoted to `int` before bit-shift, producing wrong version number (65537 instead of 1).

**Fix**: Cast final result to `uint16_t` after shifts.

**Files**: `src/engine/brush/abr_importer.cpp:31`

#### C2 — Deformation mesh bilinear interpolation error
**Symptom**: Bottom-right quadrant of deformed meshes had wrong Y coordinates.

**Root cause**: `p11.y * fy * fy` instead of `p11.y * fx * fy` in `mapPointBilinear()`.

**Fix**: Corrected the weighting factor.

**Files**: `src/engine/deformation/deformation_mesh.cpp:89`

#### C3 — `smoothstep(f)` NaN on equal edge values
**Symptom**: Brush/render functions produced NaN when `hardness >= 1.0f`.

**Root cause**: Division `(x - edge0) / (edge1 - edge0)` → 0/0 = NaN when edges equal. `clampf`/`std::clamp` propagates NaN.

**Fix**: Added `if (edge0 == edge1) return (x >= edge1) ? 1.0f : 0.0f;` guard in both copies.

**Files**: `src/core/types.hpp:218-220`, `src/engine/raster/raster_stroke.cpp:10-12`

#### C4 — Pixel byte order conflict (0xAABBGGRR vs 0xAARRGGBB)
**Symptom**: R/B channels swapped when compositor blended pixels from different engine modules.

**Root cause**: `blend_utils.hpp` used 0xAABBGGRR (R at byte 0, B at byte 2) while `raster_stroke.cpp` and QImage used 0xAARRGGBB. The compositor read correctly but blended with wrong byte order, causing channel swaps.

**Fix**: Standardized `unpackPixel`/`packPixel` in `blend_utils.hpp` to 0xAARRGGBB (Qt native). Updated compositor tests to match new byte order.

**Files**: `src/engine/common/blend_utils.hpp:12-28`, `tests/test_compositor.cpp`

#### C5 — Path traversal in audio extraction
**Symptom**: Malicious .fap file could write files outside temp directory via `../../` in `displayName`.

**Root cause**: `extractAudio()` used raw `displayName` from JSON as filename without sanitization.

**Fix**: Added `QFileInfo(rawName).fileName()` to strip path components.

**Files**: `src/io/document_manager.cpp:797-798`

### High-Severity Fixes

#### H1 — `pushFullLayerUndo` missing `hadContentBefore` (6 call sites)
**Symptom**: After undoing fill, text, move, selection, or floating selection operations, timeline cells showed empty frames even though pixel data was restored.

**Root cause**: `hadBefore = layer->hasContent()` was computed but never passed to the undo command. The `LayerModifyCommand` defaulted `hadContentBefore_ = false`, so undo always cleared `hasContent_`.

**Fix**: Pass `hadBefore` as the 3rd argument to `pushFullLayerUndo` in all 6 call sites: `doFill`, `doText`, `clearTextRaster`, `commitMove`, `commitSelection`, `commitFloatingSelection`.

**Files**: `src/ui_v2/canvas_widget_v2.cpp` (6 lines)

#### H2 — Stale sequence callbacks emitting from inactive sequences
**Symptom**: After switching sequences, the old sequence's `on_frame_changed_` callback still fired, emitting `currentFrameChanged` for the wrong sequence.

**Root cause**: `wireSequenceSignals()` only set the callback on the new active sequence but never cleared it from the old one.

**Fix**: `wireSequenceSignals()` now clears callbacks on ALL sequences before wiring the active one.

**Files**: `src/core/app_state.cpp:41-45`

#### H3 — Atomic save data loss via TOCTOU race
**Symptom**: `commitAtomic()` explicitly deleted the target file before renaming the temp file. If rename failed after delete, data was permanently lost. On Windows, `QFile::rename` already replaces the target; the explicit delete was unnecessary and dangerous.

**Fix**: Removed the explicit `QFile::remove` call. `commitAtomic` now uses `QFile::rename` directly.

**Files**: `src/io/document_manager.cpp:214-223`

#### H7 — Compositor inverted z-order (painter's algorithm)
**Symptom**: Layers were composited in reverse order — the bottom layer was drawn on top and vice versa.

**Root cause**: `compositeLayer()` iterated GroupLayer children with `rbegin()`/`rend()`, drawing the topmost layer first.

**Fix**: Changed to `begin()`/`end()` forward iteration for correct painter's algorithm (bottom→top).

**Files**: `src/engine/compositor/compositor.cpp:61`, `tests/test_compositor.cpp` (updated MoveLayerChangesOrder expectations)

#### H8 — NodeGraph dangling pointers after node removal
**Symptom**: After removing a node, other nodes' `outputs_` vectors retained raw pointers to the freed node, risking use-after-free.

**Root cause**: `removeNode()` only disconnected other nodes' inputs pointing TO the removed node, but never disconnected the removed node's own inputs (which had `outputs_` back-references in other nodes).

**Fix**: Added a second loop to disconnect all inputs of the to-be-removed node before erasing it, cleaning up `outputs_` back-references in source nodes.

**Files**: `src/engine/compositor/node_graph.cpp:329-347`

### Medium Fixes

#### M1 — Dead `Project` class
**File**: `src/core/project.cpp` — defined class entirely in .cpp with no header. Unreachable from any translation unit. Removed from filesystem and CMakeLists.

#### M8 — Dead `refreshSequenceFields`
**Files**: `src/ui_v2/property_editor_v2.cpp:950-954`, `.hpp`, `main_window_v2.cpp:421` — empty stub method still connected to `sequenceChanged` signal. Removed method body, declaration, and signal connection.

### Text Tool Fixes

| Bug | Symptom | Fix |
|-----|---------|-----|
| `textSelectionAnchor_` not reset on new entry | Weird selection behavior when clicking between text entries | Set `textSelectionAnchor_ = -1` on new entry |
| `brushColor_` desync on new entry | Text preview shown with wrong color until first keystroke | Sync `brushColor_` from `ToolState::primaryColor()` on new entry click |
| `doText` undo missing `hadContentBefore` | Timeline cells empty after undoing text placement | Pass `hadBefore` to `pushFullLayerUndo` |
| `clearTextRaster` undo missing `hadContentBefore` | Timeline cells empty after undoing text delete | Pass `hadBefore` to `pushFullLayerUndo` |

### Cleanup
- Removed dead `tests/test_timeline.cpp` (referenced nonexistent `core/timeline.hpp`)
- Removed dead `project.cpp` from `CORE_SOURCES` in CMakeLists
- Removed commented-out SEQUENCE/DURATION member variables from property_editor_v2.hpp

**Tests**: 160/160 pass.

## Bug Fix Session (v2.7 — Jul 2026)

### Non-destructive frame hiding — +/- control visibility, not deletion
**Feature**: `+`/`-` buttons in the timeline toolbar now control which frames are VISIBLE, not which exist. Frame data is never deleted when hiding.

| Button | Before | After |
|--------|--------|-------|
| `+` | Always creates new frame | Reveals hidden frame if one exists; creates new only when no hidden frames remain |
| `-` | Deletes current frame data permanently | Reduces `durationFrames_` — hides last visible frame, preserves data |

**Architecture**: `durationFrames_` (visible count) is decoupled from `totalFrames_` (data count). All navigation, playback, onion skin, and work area bounds use `durationFrames_`. Hidden frames render as dimmed cells and dimmed ruler ticks. Right-click → "Delete Frame" is the only way to permanently delete frame data.

**Model changes** (`sequence.cpp`): `setCurrentFrame`, `advanceFrame`, `effectiveWorkAreaEnd`, `nextFrame`, `prevFrame` all bind to `durationFrames_` instead of `totalFrames_`.

**Files**: `src/core/sequence.cpp`, `src/ui_v2/timeline_panel_v2.{hpp,cpp}`, `src/ui_v2/canvas_widget_v2.{hpp,cpp}`, `src/ui_v2/main_window_v2.cpp`

### Timeline Markers (After Effects-style)

**Data model** (`sequence.hpp`):
```cpp
struct Marker {
    int frame = 0;           // 0-based position
    int duration = 0;        // 0 = single-point, >0 = range marker
    std::string comment;     // Title (visible on ruler)
    std::string detail;      // Long-form notes (dialog only)
    int colorLabel = 0;      // 0-8: None, Red, Orange, Yellow, Green, Cyan, Blue, Purple, Magenta
    int64_t uid = 0;         // Unique ID
};
```

**Key constraints**: One marker per frame position (unique — adding at same frame replaces). Duration set by dragging out-point, not in dialog.

**Ruler rendering**: Colored triangles (8-12px) pointing down + semi-transparent duration bands + title labels in 7px bold white on dark pill background. 9 AE-matching colors. Ruler height increased from 18px to 22px.

**Marker Settings dialog**: Title (QLineEdit), Frame (QSpinBox, 1-based), Color Label (QComboBox, 9 colors), Detail (QPlainTextEdit, multi-line notes). Explicit dark theme stylesheet ensures all controls are readable.

**Interactions**:
| Action | Method |
|--------|--------|
| Add marker | `◆ Marker` button in timeline toolbar, or `*` (numpad) key |
| Move marker | Drag triangle on ruler |
| Set duration | Drag right edge of duration marker |
| Edit properties | Double-click marker triangle |
| Delete marker | Ctrl+click marker, or right-click → Delete Marker |
| Navigate markers | Ctrl+Shift+← / Ctrl+Shift+→ |

**Save/load** (`document_manager.cpp`): Markers serialized in `manifest.json` as JSON array per sequence. Fields: frame, duration, comment, detail, color. Backward compatible — old files load with empty detail.

**Files**: `src/core/sequence.hpp`, `src/core/sequence.cpp`, `src/io/document_manager.cpp`, `src/ui_v2/timeline_panel_v2.{hpp,cpp}`, `src/ui_v2/canvas_widget_v2.cpp`

### Marker dialog — dark theme stylesheet
**Fix**: The marker dialog now has an explicit dark theme stylesheet applied to QDialog, ensuring QSpinBox (Frame field), QLineEdit, QComboBox, QPlainTextEdit, and QPushButton all render with proper contrast (#E8ECF0 text on #13161D background, #FF4800 focus border, visible up/down arrows on QSpinBox).

**Files**: `src/ui_v2/timeline_panel_v2.cpp:574-600`

### Marker title legibility
**Fix**: Title text rendered as 7px bold white (#FFFFFF) on an opaque dark pill background (rgba 18,22,32,210) with 13px height, replacing the previous 6px colored-on-semi-transparent text that was nearly illegible.

**Files**: `src/ui_v2/timeline_panel_v2.cpp:334-353`

### updateMarker uniqueness constraint
**Fix**: `Sequence::updateMarker()` now enforces the one-marker-per-frame constraint. If moving a marker to a frame already occupied by another marker, the existing one is replaced (same logic as `addMarker`). Also added `panel_->update()` for synchronous repaint after modal dialog closes.

**Files**: `src/core/sequence.cpp:283-302`, `src/ui_v2/timeline_panel_v2.cpp`

### Marker Frame spinbox — 1-based numbering
**Fix**: The Frame field in Marker Settings now shows 1-based numbers (matching the timeline ruler). Range: 1-999999. Conversion: display = internal + 1, save = value - 1.

**Files**: `src/ui_v2/timeline_panel_v2.cpp`

**Tests**: 160/160 pass.

### Undo/Redo — missing canvasUpdated signal
**Symptom**: After pressing Ctrl+Z or Ctrl+Y, the canvas repainted correctly but the timeline cells and layer panel stayed stale — showing the previous state visually even though pixel data was restored.

**Root cause**: `CanvasWidgetV2::undo()` and `redo()` called `invalidateBackgroundCache()` + `update()` (refreshing only the canvas) but never emitted `canvasUpdated`. The `canvasUpdated` signal is connected in `MainWindowV2` to `timeline_panel_->update()` and `layer_panel_->refreshLayerList()`, so those widgets never refreshed.

**Fix**: Added `emit canvasUpdated()` in both `undo()` and `redo()`.

**Files**: `src/ui_v2/canvas_widget_v2.cpp:386-403`

**Tests**: 160/160 pass (9 undo-specific tests).

## Bug Fix Session (v2.6.2 — Jul 2026)

### H3 Regression — commitAtomic fails on Windows when target file exists
**Symptom**: After exporting a GIF (which changes the working directory via native Windows file dialog), saving the project for the first time showed "Failed to save project." The save appeared to silently fail.

**Root cause**: The H3 fix (v2.6.1) removed `QFile::remove(finalPath)` from `commitAtomic()` to prevent TOCTOU data loss, but on Windows Qt 6.5, `QFile::rename()` does **NOT** overwrite existing targets. When the save dialog confirmed overwrite of an existing file, the native dialog didn't delete it — it just returned the path. `commitAtomic` then tried to rename the `.tmp` file over the still-existing target, which failed silently on Windows.

**Fix**: `commitAtomic` now tries `QFile::rename()` first (works on Linux/macOS where it atomically replaces). If it fails and the target exists, it removes the target and retries. This is safer than the pre-H3 approach because the `remove` only happens after a failed rename, minimizing the TOCTOU window to a retry interval.

**Files**: `src/io/document_manager.cpp:214-237`

### Export resolution dialog (GIF + Video)
**Feature**: Both GIF and video export now show a resolution dialog (width × height) before encoding. Users can resize output independently of canvas dimensions. Scaling is done via `renderExportFrame` with `ExportOptions` struct.

**Files**: `src/io/video_export.hpp` (new `ExportOptions` struct), `src/io/video_export.cpp`, `src/ui_v2/main_window_v2.cpp:173-215`

### Improved save error reporting
**Fix**: Save failure error dialogs now include `dm.lastError()` text, showing the actual reason for failure (e.g., "Atomic rename failed: ...") instead of a generic "Failed to save project."

**Files**: `src/ui_v2/main_window_v2.cpp:692-696,774-778`

**Tests**: 160/160 pass.

## Bug Fix Session (v2.8 — Jul 2026)

### Video Clip Import

**Feature**: Import video clips into the timeline as tracks, similar to audio tracks. Video frames are decoded on demand via FFmpeg and composited on top of drawing layers in the canvas.

**Data model** (`document.hpp`):
```cpp
struct VideoTrackData {
    std::string filepath;      // original or temp path
    std::string displayName;   // e.g., "clip.mp4"
    std::string zipEntry;      // "video/track_0.mp4"
    int width = 0, height = 0;
    int fps = 24, totalFrames = 0;
    bool muted = false;
    int volume = 80;
    float opacity = 1.0f;
};
```

**Import limits**: Max 60 seconds, max 1920x1080. Warning dialog if exceeded (offers to trim/scale), user can cancel.

**Video decoder** (`engine/animation/video_decoder.cpp`):
- `probeVideoMetadata()`: uses `ffprobe -print_format json` to extract width, height, fps, totalFrames, duration
- `decodeVideoFrame()`: uses `ffmpeg -ss <time> -vframes 1 -f image2pipe -vcodec png -` with scale filter
- `VideoFrameCache`: LRU cache of 50 decoded full-res frames (thread-safe)

**VideoTrackWidget** (`ui_v2/video_track_widget.cpp`):
- Same button layout as AudioTrackWidget: 28x28 buttons with SVG icons (▲▼ for move, 🔊 for mute, ✕ for delete)
- Opacity slider (0-100%) + volume slider (0-100%) with matching QSlider style
- Thumbnail strip: 1 thumbnail every 30 frames (pre-decoded at 320x180 on import via FFmpeg)
- `currentFrame()`: returns decoded frame from cache or decodes on demand
- `opacityChanged()` signal → triggers canvas cache invalidation for real-time opacity preview

**Timeline integration** (`timeline_panel_v2.cpp`):
- "Add Video Track" in `+ Track ▾` menu
- `onImportVideo()`: file dialog → probe metadata → limit warnings → create widget → register with Document
- `addVideoTrackFromData()`: restores widget from loaded Document data
- `clearVideoTracks()`, `removeVideoTrack()`, `onMoveVideoTrack()`: same pattern as audio
- Non-destructive `rebuildTracks()` preserves video widgets (removeWidget/addWidget, never delete)
- `videoTrackChanged()` signal: propagated from widget opacity changes

**Canvas compositing** (`canvas_widget_v2.cpp`):
- Video frames rendered **on top** of drawing layers in `buildBackgroundCache()`
- Scaled to fit canvas, centered, maintaining aspect ratio
- Opacity from video track widget applied via `QPainter::setOpacity()`
- `setVideoTrackWidgets()`: links canvas to timeline's video widget vector
- `opacityChanged()` → `videoTrackChanged()` → invalidateBackgroundCache + repaint

**Save/Load** (`document_manager.cpp`):
- `"video"` JSON array in `timeline.json` with all metadata fields (filepath, display_name, zip_entry, width, height, fps, total_frames, muted, volume, opacity)
- Video files embedded in .fap ZIP as `video/track_N.ext` (raw bytes)
- `extractAudio()` also extracts video files to `%TEMP%/fap_video_<PID>/`
- Path sanitization: `QFileInfo().fileName()` strips directory components

**MainWindow** (`main_window_v2.cpp`):
- `syncVideoToDocument()`: captures live widget state into Document before save
- `openProject()`: restores video tracks from loaded Document
- `newProject()`: calls `clearVideoTracks()`
- Canvas wired to timeline's `videoTrackWidgets_` pointer

**Files**: `src/core/document.hpp`, `src/engine/animation/video_decoder.{hpp,cpp}`, `src/ui_v2/video_track_widget.{hpp,cpp}`, `src/ui_v2/timeline_panel_v2.{hpp,cpp}`, `src/ui_v2/canvas_widget_v2.{hpp,cpp}`, `src/ui_v2/main_window_v2.{hpp,cpp}`, `src/io/document_manager.cpp`, `CMakeLists.txt`

### Video Track Widget — button layout matching audio tracks
**Fix**: VideoTrackWidget rewritten to use `positionHeader()` with absolute positioning (matching AudioTrackWidget). Same 28x28 buttons, SVG icons, QSlider styles, and orange accent bar.

**Files**: `src/ui_v2/video_track_widget.cpp`

### Opacity slider — real-time canvas invalidation
**Fix**: Moving the opacity slider now triggers canvas background cache invalidation immediately. Signal chain: `opacityChanged()` → `videoTrackChanged()` → `canvas_->invalidateBackgroundCache()`.

**Files**: `src/ui_v2/video_track_widget.{hpp,cpp}`, `src/ui_v2/timeline_panel_v2.{hpp,cpp}`, `src/ui_v2/main_window_v2.cpp`

### Detachable Onion Skin and Canvas docks
**Feature**: Onion Skin extracted from ToolboxPanelV2 into its own `OnionSkinPanel` widget in a separate `QDockWidget`. Canvas moved from fixed `setCentralWidget()` to a detachable `QDockWidget`.

**OnionSkinPanel** (`ui_v2/onion_skin_panel.{hpp,cpp}`):
- New independent widget with QCheckBox (Enabled), two QSpinBox (Previous/Next frames, range 0-10, NoButtons), QSlider (Opacity)
- QFormLayout for aligned spinbox columns
- Emits `settingsChanged()` — writes to `ToolState` + updates canvas

**Dock layout**:
| Position | Dock | Content |
|----------|------|---------|
| Left | TOOLS | ToolboxPanelV2 (tools, canvas size) |
| Left | ONION SKIN | OnionSkinPanel |
| Right | LAYERS | LayerPanelV2 |
| Right | COLOR | ColorPanelV2 |
| Right | PROPERTIES | PropertyEditorV2 |
| Right | CANVAS SIZE | CanvasSizePanel |
| Center | CANVAS | CanvasWidgetV2 (detachable) |
| Bottom | TIMELINE | TimelinePanelV2 |

All 8 docks: `Movable | Floatable`. Orange title bar styling. Canvas wrapped in dock via `setCentralWidget(canvasDock)`.

**CanvasSizePanel** (`ui_v2/canvas_size_panel.{hpp,cpp}`):
- New independent widget with Width QSpinBox, × label, Height QSpinBox (1-8192), Apply Resize button
- Extracted from ToolboxPanelV2 into own QDockWidget ('CANVAS SIZE')
- Emits `canvasResized(int w, int h)` → toolState.setCanvasSize + canvas.resizeCanvas

**Files**: `src/ui_v2/onion_skin_panel.{hpp,cpp}` (new), `src/ui_v2/canvas_size_panel.{hpp,cpp}` (new), `src/ui_v2/toolbox_panel_v2.{hpp,cpp}`, `src/ui_v2/main_window_v2.{hpp,cpp}`, `CMakeLists.txt`

### Toolbox panel — tight fit to content
**Fix**: Reduced `setMinimumWidth` from 200px to 52px (36px icon + margins). Removed `addStretch()` so the widget uses only its natural content height. Removed QScrollArea wrapper (11 tools never overflow). Direct QVBoxLayout on the widget with no nested containers.

**Files**: `src/ui_v2/toolbox_panel_v2.cpp`

### Tablet pen pressure support (Wacom, Huion, Xencelabs, XP-Pen)
**Feature**: Canvas now responds to tablet pen pressure via Qt 6 native `QTabletEvent`. Size and opacity of brush strokes modulate in real-time based on pen pressure. Tablet eraser detection auto-switches to eraser mode.

**Implementation** (`canvas_widget_v2.cpp`):
- `WA_TabletTracking` enabled in constructor
- `tabletEvent()` override handles TabletPress/Move/Release + proximity events
- `tabletPressure_` (0.0–1.0) extracted from `QTabletEvent::pressure()`
- `tabletEraser_` detected via `QPointingDevice::PointerType::Eraser`
- `drawBrushStamp()`: radius modulated by pressure (20% min, 100% max), opacity multiplied by pressure
- Falls back to mouse behavior (pressure=1.0) when no tablet in proximity
- No external SDKs required — uses Qt 6 native tablet API

**Files**: `src/ui_v2/canvas_widget_v2.{hpp,cpp}`

**Tests**: 160/160 pass.

### Video + drawing — partial rebuild escalation
**Symptom**: When drawing on a raster layer with a video track active, white rectangles appeared over the video area. Switching frames made them disappear and the drawing was preserved.

**Root cause**: `commitStroke()` called `buildBackgroundCache(commitR)` (partial rebuild). The partial path filled the dirty rect with white and composited drawing layers, but video frames were only in the FULL rebuild path. The white fill was visible through the stroke overlay.

**Fix**: When video tracks are active, partial rebuilds are escalated to full rebuilds. The partial path cannot correctly composite video frames (they need full canvas context). Only affects stroke commits (once per stroke), not per-dab rendering.

**Files**: `src/ui_v2/canvas_widget_v2.cpp:767-772`

### Audio/Video track layout position preserved on save/load
**Symptom**: Moving a video track above drawing sequences in the timeline, then saving and reopening, placed the video track back at the bottom (below sequences and audio).

**Root cause**: `rebuildTracks()` and `addAudioTrackFromData`/`addVideoTrackFromData` always inserted tracks at the end of the layout. The track's layout position was not saved in the Document.

**Fix**: Added `layoutPosition` field to `AudioTrackData` and `VideoTrackData`. `syncAudioToDocument`/`syncVideoToDocument` capture `tracksLayout_->indexOf(widget)`. On load, `insertWidget` at saved position. Backward compatible (default 9999 = append at end).

**Files**: `src/core/document.hpp`, `src/io/document_manager.cpp`, `src/ui_v2/timeline_panel_v2.{hpp,cpp}`, `src/ui_v2/main_window_v2.cpp`

**Tests**: 160/160 pass.

## Bug Fix Session (v2.9 — Jul 2026)

### Line Boil — Efecto de linea vibrante por secuencia

**Feature**: Efecto no destructivo de "linea vibrante" (line boil) aplicado por secuencia. Las lineas dibujadas se desplazan ligeramente con ruido posicional cada frame, creando el aspecto organico de animacion tradicional.

**Algoritmo**: Ruido bilineal por celdas de 8x8 pixeles, desplazamiento maximo controlable por slider 0-10px. El indice de frame actua como semilla — patron de ruido consistente por frame, diferente entre frames.

**Arquitectura**:

| Capa | Implementacion |
|------|---------------|
| Data model | `LineBoil { enabled, strength, seed }` en `Sequence` (`sequence.hpp:37-42`) |
| Engine | `applyLineBoil(QImage&, frame, strength, seed)` en `engine/raster/raster_effect.cpp` |
| Display | Hook en `buildBackgroundCache()` FULL + PARTIAL paths (`canvas_widget_v2.cpp`) |
| Export | Hook en `renderExportFrame()` (`video_export.cpp`) |
| UI | Checkbox "Boil" + slider 0-100 en `SequenceTrackWidget` |
| Serialization | `line_boil_enabled` + `line_boil_strength` en `timeline.json` |
| AppState | `setLineBoilEnabled()` / `setLineBoilStrength()` → `emit documentChanged()` |

**Invariantes**:
- Efecto NO destructivo — aplicado en dominio DISPLAY sobre copia de QImage, nunca modifica `pixelBuffer_`
- Canvas ya conectado a `documentChanged` → `invalidateBackgroundCache()` automatico
- Slider deshabilitado hasta que checkbox esta checked
- Backward compatible: campos nuevos en JSON son opcionales, default `enabled=false, strength=2.0`

**Files**: `core/sequence.{hpp,cpp}`, `engine/raster/raster_effect.{hpp,cpp}`, `ui_v2/canvas_widget_v2.cpp`, `io/video_export.cpp`, `ui_v2/timeline_panel_v2.cpp`, `io/document_manager.cpp`, `core/app_state.{hpp,cpp}`, `CMakeLists.txt`

### Sequence Lock — Candado por secuencia con feedback visual

**Fix**: El candado de secuencia ahora es claramente visible y bloquea correctamente el dibujo.

**Problemas corregidos**:

| Issue | Antes | Despues |
|-------|-------|---------|
| Boton lock | Mismo color naranja en ambos estados | `:checked` → fondo rojo `#FF4A4A`, `:!checked` → fondo `kBtnPressed` |
| Celdas bloqueadas | Identicas a desbloqueadas | Overlay rojo semi-transparente `rgba(255,74,74,50)` |
| `isSequenceLocked()` | Solo verificaba `activeSequence()` | Verifica `activeSequence()` — candado por secuencia individual |

**Invariantes**:
- Nueva secuencia: desbloqueada por defecto (`locked_ = false`)
- Desbloquear no borra pixel data
- Estado `locked` ya serializado en `timeline.json` (campo existente)
- Canvas bloquea Brush, Eraser, Fill, Text, Move, Select, Line, Rect, Ellipse — permite Hand y ColorPicker

**Files**: `ui_v2/timeline_panel_v2.cpp`, `ui_v2/canvas_widget_v2.cpp`

### Frame keyboard shortcuts — `+` y `-` para duplicar/ocultar frames

**Feature**: Atajos de teclado para manipulacion de frames. Al mantener presionada la tecla, Qt auto-repeat ejecuta la accion secuencialmente.

| Tecla | Accion |
|-------|--------|
| `+` o `=` | Duplicar frame (revela oculto primero, crea nuevo si todos visibles) |
| `-` | Ocultar frame (reduce `durationFrames_`, preserva datos) |

**Arquitectura**:
- Canvas emite `addFrameRequested()` / `hideFrameRequested()`
- `MainWindowV2` enruta a `TimelinePanelV2::addFrame()` / `hideFrame()` (ahora publicos)

**Files**: `ui_v2/canvas_widget_v2.{hpp,cpp}`, `ui_v2/main_window_v2.cpp`, `ui_v2/timeline_panel_v2.{hpp,cpp}`

**Tests**: 160/160 pass.
