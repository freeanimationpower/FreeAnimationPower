# Changelog — Free Animation Power v2.0.0

## [Unreleased] — 2026-07-02

### Text Tool Bugfix Triad

**Fixed: Hit-test blind to text body**
- Replaced static `dx<12 && dy<24` anchor-point hit-test with `TextEntry::boundingRect()` using `QFontMetrics::horizontalAdvance()` + `ascent/descent`. Full-text click zone with 4px tolerance. Works for any text length or font size.

**Fixed: Immortal text entries (no delete)**
- Moved `entries.erase()` + `clearTextRaster()` outside `if (!text.isEmpty())` guard in `commitTextEdit()`. Emptying text + Enter now properly deletes.
- Added Delete/Backspace handler in `keyPressEvent` for Text tool when NOT editing. Finds closest entry to last click point and deletes raster + metadata.
- `clearTextRaster()`: canvas→layer-local coordinate mapping via `originX/Y`, `QFontMetrics` bounding box, zero-fill with undo snapshot.

**Fixed: Caret drawn too low (misaligned)**
- Changed `fm.ascent() * 0.2` → `fm.ascent()` and `fm.descent() * 1.1` → `fm.descent()` in caret calculation. Caret now spans the full text top-to-bottom, eliminating the ~80% gap visible with large fonts (72pt).

**Added:**
- `TextEntry::boundingRect()` helper method
- `QPointF lastTextClickPos_` member
- `deleteLastClickedTextEntry()` and `clearTextRaster()` helper functions
- `#include <limits>` for `std::numeric_limits`

**Files changed:**
- `src/ui_v2/canvas_widget_v2.hpp`: struct TextEntry extended, 3 new private members
- `src/ui_v2/canvas_widget_v2.cpp`: 6 code sections modified, 2 new functions (109 insertions, 12 deletions)

### Previous Changes

#### 2026-07-02
- **Middle mouse button canvas panning** — independent of active tool, works with any tool selected
- **Layer panel display desync fix** — signal separation (`layerChanged` vs `layerDisplayPropertiesChanged`)
- **4-buffer CPU display pipeline** — strict DATA/DISPLAY separation, dirty rect partial rebuilds, isolated stroke buffer

#### 2026-06-27
- Memory stress test infrastructure (`test_memory_stress.cpp`, `MEMORY_REPORT.md`)
- Layer origin offset support for `buildBackgroundCache(rect)` partial rebuilds
- GPLv3 license file

#### 2026-06-26
- Text tool redesign: custom font picker, Photoshop-inspired character panel, live inline typing, multi-line with alignment/leading/tracking, undo support
- Typography controls added to `ToolState` (8 Q_PROPERTY text properties)
- `focusOutEvent` fix: no longer commits when focus moves to app panels
- `doText()` layer origin correction
- `ToolState::resetToDefaults()` called on startup

#### Earlier (multiple commits)
- Brush undo ghost outlines fix
- Ghost rectangle artifact on brush strokes fix
- Aggressive buffer expansion fix
- Content clipping on move fix
- Qt raster tile seams fix
- Undo capture rect mismatch fix
- Guard band & alpha compositing
- Keyboard shortcuts & selection operations
- Full V2 tool implementation (11 tools)
- Brush controls unification in PropertyEditorV2
