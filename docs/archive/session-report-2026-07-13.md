# Session Report — 2026-07-13

## Summary
v2.6 session: 6 bug fixes + 2 new features. 160 tests passing.

## Bug Fixes

| # | Bug | Root Cause | Fix |
|---|-----|-----------|-----|
| 12 | Eraser produces no effect | `drawBrushStamp()` applied `DestinationOut`-like blend on transparent `strokeBuffer_` | Unified SourceOver blend for both brush/eraser stamps |
| 13 | Layer names overlap visually | `QListWidget::clear()` doesn't delete `setItemWidget()` widgets | `removeItemWidget()` + `hide()` + `deleteLater()` before `clear()` |
| 14 | Crash during layer rename | Raw pointers in `startRename()` lambda captured to deleted widgets | `QPointer<QHBoxLayout>`, `QPointer<QLineEdit>`, `QPointer<QLabel>` |
| 15 | Video export duplicated + no audio | Format detection existed in two places, no audio mixing | Unified `exportVideo()`, added `amix` audio mixing |
| 16 | Playback timer imprecision | `1000/fps` integer division (41ms vs 41.67ms at 24fps) | `std::round(1000.0/fps)` floating-point |
| Playback stuck | Post-load `rootLayerForFrame(f)` guard broke frames without JSON | Reverted to original behavior |

## New Features

### Copy Layer to All Frames (#17)
- `Sequence::copyLayerToAllFrames(int sourceFrame, int layerIndex)` — copies metadata + pixels
- `CopyLayerToFramesCommand` — undo/redo with per-frame backups
- UI button in LayerPanelV2 footer (duplicate icon)

### .fap File Icon + Association (#18)
- `resources/icons/fap.ico` — 7 resolutions (16-256px)
- `scripts/embed_icon.py` — Win32 `BeginUpdateResource` post-build injector
- `src/platform/file_association.{hpp,cpp}` — registry-based `.fap` handler

## Video Export Unification
- `exportVideo(doc, path, fps)` — auto-detects format from extension
- Audio mixing via FFmpeg `amix` filter with per-track volume
- MOV QuickTime Alpha (qtrle+argb), WebM VP9 Alpha (libvpx-vp9+yuva420p)
- GIF: `MainWindowV2` now calls `fap::exportGIF()` (was 60 lines duplicate)
- `exportGIF()`: removed hardcoded 320px scale, full canvas resolution

## Files Modified
| File | Changes |
|------|---------|
| `src/core/sequence.hpp` | + copyLayerToAllFrames declaration |
| `src/core/sequence.cpp` | + copyLayerToAllFrames implementation |
| `src/io/video_export.hpp` | Simplified exportVideo signature |
| `src/io/video_export.cpp` | Unified exportVideo + audio mixing + exportGIF fix |
| `src/io/document_manager.cpp` | Reverted post-load guard |
| `src/ui_v2/canvas_widget_v2.cpp` | Eraser blend fix |
| `src/ui_v2/layer_panel_v2.hpp` | + copyLayerToAllFrames slot |
| `src/ui_v2/layer_panel_v2.cpp` | Widget cleanup fix + QPointer guards + copy button + CopyLayerToFramesCommand |
| `src/ui_v2/main_window_v2.cpp` | Simplified exportVideo + exportGIF delegation + file association |
| `src/ui_v2/timeline_panel_v2.cpp` | Timer precision fix |
| `CMakeLists.txt` | Icon embedding post-build step |
| `resources/resources.qrc` | + fap.ico entry |

## New Files
- `resources/icons/fap.ico` — multi-resolution app icon
- `scripts/embed_icon.py` — Win32 icon resource injector
- `src/platform/file_association.hpp` — registry association header
- `src/platform/file_association.cpp` — registry association implementation

## Test Results
- 160/160 tests pass
- No regressions detected

## Known Limitations
- Playback at 1080p with 4+ layers may stutter due to `buildBackgroundCache()` full rebuild per frame
- No auto-save/recovery mechanism
- Layer name changes don't propagate across frames (by design — per-frame layer model)
